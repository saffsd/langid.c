// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//
// Author: Marco Lui <saffsd@gmail.com>
//   based very closely on compact_lang_det_test.cc by dsites@google.com (Dick Sites)
//

// Test: Read each line as a path and do detection thereon

#include <math.h>                   // for sqrt
#include <stdlib.h>                 // for exit
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>               // for mmap
#include <unistd.h>                 // for getline
#include <fcntl.h>
#include <string>

#include "cld2tablesummary.h"
#include "compact_lang_det_impl.h"
#include "debug.h"
#include "integral_types.h"
#include "lang_script.h"
#include "utf8statetable.h"

namespace CLD2 {

using namespace std;

// Scaffolding
typedef int32 Encoding;
static const Encoding UNKNOWN_ENCODING = 0;

// Linker supplies the right tables; see ScoringTables compact_lang_det_impl.cc
// These are here JUST for printing versions
extern const UTF8PropObj cld_generated_CjkUni_obj;
extern const CLD2TableSummary kCjkDeltaBi_obj;
extern const CLD2TableSummary kDistinctBiTable_obj;
extern const CLD2TableSummary kQuad_obj;
extern const CLD2TableSummary kDeltaOcta_obj;
extern const CLD2TableSummary kDistinctOcta_obj;
extern const CLD2TableSummary kOcta2_obj;
extern const short kAvgDeltaOctaScore[];

bool FLAGS_plain = false;
bool FLAGS_dbgscore = true;


// Convert GetTimeOfDay output to 64-bit usec
static inline uint64 Microseconds(const struct timeval& t) {
  // Convert to (uint64) microseconds,  not (double) seconds.
  return t.tv_sec * 1000000ULL + t.tv_usec;
}

#define LF 0x0a
#define CR 0x0d

bool Readline(FILE* infile, char* buffer) {
  char* p = fgets(buffer, 64 * 1024, infile);
  if (p == NULL) {
    return false;
  }
  int len = strlen(buffer);

  // trim CR LF
  if (buffer[len-1] == LF) {buffer[--len] = '\0';}
  if (buffer[len-1] == CR) {buffer[--len] = '\0';}
  return true;
}

bool IsComment(char* buffer) {
  int len = strlen(buffer);
  if (len == 0) {return true;}
  if (buffer[0] == '#') {return true;}
  if (buffer[0] == ' ') {return true;}    // Any leading space is comment
  return false;
}



void DumpExtLang(int flags,
                 Language summary_lang,
                 Language* language3, int* percent3,
                 double* normalized_score3,
                 int text_bytes, bool is_reliable, int in_size) {
  char temp[160];
  char* tp = temp;
  int tp_left = sizeof(temp);
  snprintf(tp, tp_left, "ExtLanguage");

  if (language3[0] != UNKNOWN_LANGUAGE) {
    tp = temp + strlen(temp);
    tp_left = sizeof(temp) - strlen(temp);
    snprintf(tp, tp_left, " %s(%d%% %3.0fp)",
             LanguageName(language3[0]),
             percent3[0],
             normalized_score3[0]);

  }
  if (language3[1] != UNKNOWN_LANGUAGE) {
    tp = temp + strlen(temp);
    tp_left = sizeof(temp) - strlen(temp);
    snprintf(tp, tp_left, ", %s(%d%% %3.0fp)",
             LanguageName(language3[1]),
             percent3[1],
             normalized_score3[1]);
  }
  if (language3[2] != UNKNOWN_LANGUAGE) {
    tp = temp + strlen(temp);
    tp_left = sizeof(temp) - strlen(temp);
    snprintf(tp, tp_left, ", %s(%d%% %3.0fp)",
             LanguageName(language3[2]),
             percent3[2],
             normalized_score3[2]);
  }

  if (text_bytes > 9999) {
    tp = temp + strlen(temp);
    tp_left = sizeof(temp) - strlen(temp);
    snprintf(tp, tp_left, ", %d/%d KB of non-tag letters",
            text_bytes >> 10, in_size >> 10);
  } else {
    tp = temp + strlen(temp);
    tp_left = sizeof(temp) - strlen(temp);
    snprintf(tp, tp_left, ", %d/%d bytes of non-tag letters",
            text_bytes, in_size);
  }

  tp = temp + strlen(temp);
  tp_left = sizeof(temp) - strlen(temp);
  snprintf(tp, tp_left, ", Summary: %s%s",
           LanguageName(summary_lang),
           is_reliable ? "" : "*");

  printf("%s\n", temp);

}

void DumpLanguages(Language summary_lang,
                   Language* language3, int* percent3,
                 int text_bytes, bool is_reliable, int in_size) {
  // fprintf(stderr, "</span>\n\n");
  int total_percent = 0;
  if (language3[0] != UNKNOWN_LANGUAGE) {
    fprintf(stderr, "\n<br>Languages %s(%d%%)",
            LanguageName(language3[0]),
            percent3[0]);
    total_percent += percent3[0];
  } else {
    fprintf(stderr, "\n<br>Languages ");
  }

  if (language3[1] != UNKNOWN_LANGUAGE) {
    fprintf(stderr, ", %s(%d%%)",
            LanguageName(language3[1]),
            percent3[1]);
    total_percent += percent3[1];
  }

  if (language3[2] != UNKNOWN_LANGUAGE) {
    fprintf(stderr, ", %s(%d%%)",
            LanguageName(language3[2]),
            percent3[2]);
    total_percent += percent3[2];
  }

  fprintf(stderr, ", other(%d%%)", 100 - total_percent);

  if (text_bytes > 9999) {
    fprintf(stderr, ", %d/%d KB of non-tag letters",
            text_bytes >> 10, in_size >> 10);
  } else {
    fprintf(stderr, ", %d/%d bytes of non-tag letters",
            text_bytes, in_size);
  }

  fprintf(stderr, ", Summary: %s%s ",
          LanguageName(summary_lang),
          is_reliable ? "" : "*");
  fprintf(stderr, "<br>\n");
}


int main(int argc, char** argv) {
  int flags = 0;
  bool get_vector = false;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--scoreasquads") == 0) {flags |= kCLDFlagScoreAsQuads;}
    if (strcmp(argv[i], "--cr") == 0) {flags |= kCLDFlagCr;}
    if (strcmp(argv[i], "--verbose") == 0) {flags |= kCLDFlagVerbose;}
    if (strcmp(argv[i], "--echo") == 0) {flags |= kCLDFlagEcho;}
    if (strcmp(argv[i], "--vector") == 0) {get_vector = true;}
  }


  const char* tldhint = "";
  Encoding enchint = UNKNOWN_ENCODING;
  Language langhint = UNKNOWN_LANGUAGE;

  int bytes_consumed;
  int bytes_filled;
  int error_char_count;
  bool is_reliable;
  char* buffer = new char[10000000];  // Max 10MB of input for this test program


  // Full-blown flag-bit and hints interface
  bool allow_extended_lang = true;
  Language plus_one = UNKNOWN_LANGUAGE;

  bool ignore_7bit = false;


  // Detect language
  Language summary_lang = UNKNOWN_LANGUAGE;

  Language language3[3];
  int percent3[3];
  double normalized_score3[3];
  ResultChunkVector resultchunkvector;
  bool is_plain_text = FLAGS_plain;
  int text_bytes;

  CLDHints cldhints = {NULL, tldhint, enchint, langhint};

  int fd;
  unsigned textlen, pathlen;
  char *path = NULL, *text = NULL;
  size_t path_size = 4096;


  // Main Loop
  while ((pathlen = getline(&path, &path_size, stdin)) != -1){
    path[pathlen-1] = '\0';
    if ((fd = open(path, O_RDONLY))==-1) {
      fprintf(stderr, "unable to open %s\n", path);
      exit(-1);
    }

    textlen = lseek(fd, 0, SEEK_END);
    text = (char *) mmap(NULL, textlen, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    //lang = identify(lid, text, textlen);

    summary_lang = CLD2::DetectLanguageSummaryV2(
                          text,
                          textlen,
                          is_plain_text,
                          &cldhints,
                          allow_extended_lang,
                          flags,
                          plus_one,
                          language3,
                          percent3,
                          normalized_score3,
                          get_vector ? &resultchunkvector : NULL,
                          &text_bytes,
                          &is_reliable);

      if (get_vector) {
        DumpResultChunkVector(stderr, buffer, &resultchunkvector);
      }

      //DumpExtLang(flags, summary_lang, language3, percent3, normalized_score3,
      //            text_bytes, is_reliable, textlen);

      /*
      printf("  SummaryLanguage %s%s at %u of %d, %s\n",
            LanguageName(summary_lang),
            is_reliable ? "" : "(un-reliable)",
            bytes_consumed,
            textlen,
            path);
      */

      printf("%s,%d,%s,%s\n", path, textlen, LanguageCode(summary_lang), LanguageName(summary_lang));


      /* no need to munmap if textlen is 0 */
      if (textlen && (munmap(text, textlen) == -1)) {
        fprintf(stderr, "failed to munmap %s of length %d \n", path, textlen);
        exit(-1);
      }

      close(fd);
    }


  return 0;
}

}       // End namespace CLD2

int main(int argc, char *argv[]) {
  return CLD2::main(argc, argv);
}

