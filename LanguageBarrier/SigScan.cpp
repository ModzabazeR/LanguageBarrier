#include "LanguageBarrier.h"
#include <dbghelp.h>
#include <malloc.h>
#include <string>
#include <iomanip>
#include "Config.h"
#include "SigExpr.h"

// Stolen from
// https://raw.githubusercontent.com/learn-more/findpattern-bench/master/patterns/atom0s_mrexodia.h

namespace {
using namespace std;

struct PatternByte {
  struct PatternNibble {
    unsigned char data;
    bool wildcard;
  } nibble[2];
};

static string FormatPattern(string patterntext) {
  string result;
  int len = patterntext.length();
  for (int i = 0; i < len; i++)
    if (patterntext[i] == '?' || isxdigit(patterntext[i]))
      result += toupper(patterntext[i]);
  return result;
}

static int HexChToInt(char ch) {
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  else if (ch >= 'A' && ch <= 'F')
    return ch - 'A' + 10;
  else if (ch >= 'a' && ch <= 'f')
    return ch - 'a' + 10;
  return 0;
}

static bool TransformPattern(string patterntext, vector<PatternByte>& pattern) {
  pattern.clear();
  patterntext = FormatPattern(patterntext);
  int len = patterntext.length();
  if (!len) return false;

  if (len % 2)  // not a multiple of 2
  {
    patterntext += '?';
    len++;
  }

  PatternByte newByte;
  for (int i = 0, j = 0; i < len; i++) {
    if (patterntext[i] == '?')  // wildcard
    {
      newByte.nibble[j].wildcard = true;  // match anything
    } else                                // hex
    {
      newByte.nibble[j].wildcard = false;
      newByte.nibble[j].data = HexChToInt(patterntext[i]) & 0xF;
    }

    j++;
    if (j == 2)  // two nibbles = one byte
    {
      j = 0;
      pattern.push_back(newByte);
    }
  }
  return true;
}

static bool MatchByte(const unsigned char byte, const PatternByte& pbyte) {
  int matched = 0;

  unsigned char n1 = (byte >> 4) & 0xF;
  if (pbyte.nibble[0].wildcard)
    matched++;
  else if (pbyte.nibble[0].data == n1)
    matched++;

  unsigned char n2 = byte & 0xF;
  if (pbyte.nibble[1].wildcard)
    matched++;
  else if (pbyte.nibble[1].data == n2)
    matched++;

  return (matched == 2);
}

uintptr_t FindPattern(const unsigned char* dataStart, const unsigned char* dataEnd, const char* pszPattern,
                      uintptr_t baseAddress, size_t offset, int occurrence) {
  // Build vectored pattern..
  vector<PatternByte> patterndata;
  if (!TransformPattern(pszPattern, patterndata)) return NULL;

  // The result count for multiple results..
  int resultCount = 0;
  const unsigned char* scanStart = dataStart;

  while (true) {
    // Search for the pattern..
    const unsigned char* ret =
        search(scanStart, dataEnd, patterndata.begin(), patterndata.end(),
               MatchByte);

    // Did we find a match..
    if (ret != dataEnd) {
      // If we hit the usage count, return the result..
      if (occurrence == 0 || resultCount == occurrence)
        return baseAddress + distance(dataStart, ret) + offset;

      // Increment the found count and scan again..
      resultCount++;
      scanStart = ++ret;
    } else
      break;
  }

  return NULL;
}
}  // namespace

namespace lb {
uintptr_t sigScanRaw(const char* category, const char* sigName, bool isData = false) {
  std::stringstream logstr;
  logstr << "SigScan: looking for " << category << "/" << sigName << "... "
         << std::endl;

  json sig = config["gamedef"]["signatures"][category][sigName];
  std::string sPattern = sig["pattern"].get<std::string>();
  const char* pattern = sPattern.c_str();
  size_t offset = sig["offset"].get<size_t>();

  logstr << sPattern << std::endl;

  HMODULE hModule = GetModuleHandleA(category);
  if (!hModule) hModule = GetModuleHandle(NULL);
  IMAGE_NT_HEADERS* pNtHdr = ImageNtHeader(hModule);
  IMAGE_SECTION_HEADER* pSectionHdr =
      (IMAGE_SECTION_HEADER*)((uint8_t*)&(pNtHdr->OptionalHeader) +
                              pNtHdr->FileHeader.SizeOfOptionalHeader);

  for (size_t i = 0; i < pNtHdr->FileHeader.NumberOfSections; i++) {
    if (isData == !!(pSectionHdr->Characteristics & IMAGE_SCN_MEM_EXECUTE))
      continue;

    uintptr_t baseAddress = (uintptr_t)hModule + pSectionHdr->VirtualAddress;
    uintptr_t retval = (uintptr_t)FindPattern(
        (unsigned char*)baseAddress,
        (unsigned char*)baseAddress + pSectionHdr->Misc.VirtualSize,
        pattern, baseAddress, offset, sig["occurrence"].get<int>());

    if (retval != NULL) {
      logstr << " found at 0x" << std::hex << retval;
      if (lb::IsInitialised) {
        LanguageBarrierLog(logstr.str());
      }
      return retval;
    }
    pSectionHdr++;
  }
  logstr << " not found!";
  if (lb::IsInitialised) {
    LanguageBarrierLog(logstr.str());
  }
  return NULL;
}

uintptr_t sigScan(const char* category, const char* sigName, bool isData = false) {
  uintptr_t raw = sigScanRaw(category, sigName, isData);
  json sig = config["gamedef"]["signatures"][category][sigName];
  if (sig.count("expr") == 0) return raw;

  try {
    return SigExpr(sig["expr"].get<std::string>(), raw).evaluate();
  } catch (std::runtime_error& e) {
    LanguageBarrierLog(e.what());
    return NULL;
  }
}
}  // namespace lb