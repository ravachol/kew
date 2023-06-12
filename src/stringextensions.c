#include "stringextensions.h"

char* stringToLower(char* str) 
{
    for (int i = 0; str[i] != '\0'; i++) {
        str[i] = tolower(str[i]);
    }
    return str;
}

char* strcasestr(const char* haystack, const char* needle) 
{
  if (!haystack || !needle)
    return NULL;

  size_t needleLen = strlen(needle);
  size_t haystackLen = strlen(haystack);

  if (needleLen > haystackLen)
    return NULL;

  for (size_t i = 0; i <= haystackLen - needleLen; i++) {
    size_t j;
    for (j = 0; j < needleLen; j++) {
      if (tolower(haystack[i + j]) != tolower(needle[j]))
        break;
    }
    if (j == needleLen)
      return (char*)(haystack + i);
  }

  return NULL;
}

int match_regex(regex_t* regex, const char* ext) 
{
    if (regex == NULL || ext == NULL) {
        fprintf(stderr, "Invalid arguments\n");
        return 1;
    }
    regmatch_t pmatch[1];  // Array to store match results
    int ret = regexec(regex, ext, 1, pmatch, 0);
    if (ret == REG_NOMATCH) {
        return 1;
    } else if (ret == 0) {
        return 0;
    } else {
        char errorBuf[100];
        regerror(ret, regex, errorBuf, sizeof(errorBuf));
        fprintf(stderr, "Regex match failed: %s\n", errorBuf);
        return 1;
    }
}

void extractExtension(const char* filename, size_t numChars, char* ext) 
{
    size_t length = strlen(filename);
    size_t copyChars = length < numChars ? length : numChars;

    const char* extStart = filename + length - copyChars;
    strncpy(ext, extStart, copyChars);
    ext[copyChars] = '\0';
}

int endsWith(const char* str, const char* suffix) 
{
    size_t strLength = strlen(str);
    size_t suffixLength = strlen(suffix);

    if (suffixLength > strLength) {
        return 0;  // The suffix is longer than the string, it cannot be a suffix
    }

    const char* strSuffix = str + (strLength - suffixLength);
    return strcmp(strSuffix, suffix) == 0;
}