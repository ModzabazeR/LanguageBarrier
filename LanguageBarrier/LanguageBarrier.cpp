#define DEFINE_JSON_CONSTANTS
#include "LanguageBarrier.h"
#include <ctime>
#include <fstream>
#include "Config.h"
#include "Game.h"
#include "GameText.h"
#include "MinHook.h"
#include "Script.h"
#include "SigScan.h"

namespace lb {

bool IsConfigured = false;
bool IsInitialised = false;

void *memset_perms(void *dst, int val, size_t size) {
  DWORD oldProtect;
  VirtualProtect(dst, size, PAGE_READWRITE, &oldProtect);
  void *retval = memset(dst, val, size);
  VirtualProtect(dst, size, oldProtect, &oldProtect);
  return retval;
}
void *memcpy_perms(void* dst, const void* src, size_t size) {
  DWORD oldProtect;
  VirtualProtect(dst, size, PAGE_READWRITE, &oldProtect);
  void *retval = memcpy(dst, src, size);
  VirtualProtect(dst, size, oldProtect, &oldProtect);
  return retval;
}
size_t alignCeil(size_t val, size_t align) {
  return (val % align == 0) ? val : val + align - (val % align);
}
std::string WideTo8BitPath(std::wstring const &wide) {
  if (wide.empty()) return std::string();

  wchar_t const *convSrc = wide.c_str();
  char *buffer;
  wchar_t *shortBuffer;
  int sz;
  int shortSz;

  UINT codepage = AreFileApisANSI() ? CP_ACP : CP_OEMCP;

  // If path is longer than MAX_PATH or contains unrepresentable characters,
  // it cannot be used with ANSI Windows APIs
  // Short paths (DOS 8.3) can always be converted properly, and hopefully
  // used...
  BOOL needShortPath;

  if (wide.size() > MAX_PATH) {
    // Might not *help*, Windows gets *weird* beyond this limit, but...
    needShortPath = TRUE;
  } else {
    sz = WideCharToMultiByte(codepage, WC_NO_BEST_FIT_CHARS, convSrc, -1, NULL,
                             0, NULL, &needShortPath);
  }

  if (needShortPath) {
    shortSz = GetShortPathNameW(convSrc, NULL, 0);
    shortBuffer = (wchar_t *)_malloca(shortSz * sizeof(wchar_t));

    GetShortPathNameW(convSrc, shortBuffer, shortSz);

    sz = WideCharToMultiByte(codepage, WC_NO_BEST_FIT_CHARS, convSrc, -1, NULL,
                             0, NULL, NULL);

    convSrc = shortBuffer;
  }

  buffer = (char *)_malloca(sz);
  WideCharToMultiByte(codepage, WC_NO_BEST_FIT_CHARS, convSrc, -1, buffer, sz,
                      NULL, NULL);
  std::string result(buffer);

  if (needShortPath) {
    _freea(shortBuffer);
  }
  _freea(buffer);

  return result;
}
std::wstring GetGameDirectoryPath() {
  int sz = MAX_PATH;

  wchar_t *buffer = (wchar_t *)_malloca(sz * sizeof(wchar_t));
  GetModuleFileNameW(NULL, buffer, sz);
  while (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    _freea(buffer);
    sz *= 2;
    buffer = (wchar_t *)_malloca(sz * sizeof(wchar_t));
    GetModuleFileNameW(NULL, buffer, sz);
  }

  int dirlen = max(wcsrchr(buffer, L'\\'), wcsrchr(buffer, L'/')) - buffer;

  std::wstring result(buffer, dirlen);

  _freea(buffer);

  return result;
}
void loadJsonConstants() {
  LanguageBarrierLog("loading constants from gamedef.json/patchdef.json...");

  // Game.h
  BGM_CLEAR = config["gamedef"]["bgmClear"].get<uint32_t>();
  MPK_ID_SCRIPT_MPK = config["gamedef"]["mpkIdScriptMpk"].get<uint8_t>();
  MPK_ID_BGM_MPK = config["gamedef"]["mpkIdBgmMpk"].get<uint8_t>();
  AUDIO_PLAYER_ID_BGM1 = config["gamedef"]["audioPlayerIdBgm1"].get<uint8_t>();

  // GameText.h
  FIRST_FONT_ID = config["gamedef"]["firstFontId"].get<uint8_t>();
  COORDS_MULTIPLIER = config["gamedef"]["coordsMultiplier"].get<float>();
  FONT_CELL_WIDTH = config["gamedef"]["fontCellWidth"].get<uint8_t>();
  FONT_CELL_HEIGHT = config["gamedef"]["fontCellHeight"].get<uint8_t>();
  FONT_ROW_LENGTH = config["gamedef"]["fontRowLength"].get<uint8_t>();
  GLYPH_RANGE_FULLWIDTH_START =
      config["gamedef"]["glyphRangeFullwidthStart"].get<uint16_t>();
  DEFAULT_LINE_LENGTH = config["gamedef"]["defaultLineLength"].get<uint16_t>();
  DEFAULT_MAX_CHARACTERS =
      config["gamedef"]["defaultMaxCharacters"].get<uint16_t>();
  HAS_SGHD_PHONE = config["gamedef"]["hasSghdPhone"].get<bool>();
  if (HAS_SGHD_PHONE) {
    SGHD_LINK_UNDERLINE_GLYPH_X =
        config["gamedef"]["sghdLinkUnderlineGlyphX"].get<float>();
    SGHD_LINK_UNDERLINE_GLYPH_Y =
        config["gamedef"]["sghdLinkUnderlineGlyphY"].get<float>();
    SGHD_PHONE_X_PADDING = config["patch"]["sghdPhoneXPadding"].get<int>();
  }
  // these are default true for backwards compatibility with S;G0 patch config
  HAS_DOUBLE_GET_SC3_STRING_DISPLAY_WIDTH = true;
  if (config["gamedef"].count("hasDoubleGetSc3StringDisplayWidth") == 1) {
    HAS_DOUBLE_GET_SC3_STRING_DISPLAY_WIDTH =
        config["gamedef"]["hasDoubleGetSc3StringDisplayWidth"].get<bool>();
  }
  HAS_DRAW_PHONE_TEXT = true;
  if (config["gamedef"].count("hasDrawPhoneText") == 1) {
    HAS_DRAW_PHONE_TEXT = config["gamedef"]["hasDrawPhoneText"].get<bool>();
  }
  HAS_GET_SC3_STRING_LINE_COUNT = true;
  if (config["gamedef"].count("hasGetSc3StringLineCount") == 1) {
    HAS_GET_SC3_STRING_LINE_COUNT =
        config["gamedef"]["hasGetSc3StringLineCount"].get<bool>();
  }
  HAS_RINE = config["gamedef"]["hasRine"].get<bool>();
  if (HAS_RINE) {
    RINE_BLACK_NAMES = config["patch"]["rineBlackNames"].get<bool>();
  }
  DIALOGUE_REDESIGN_YOFFSET_SHIFT =
      config["patch"]["dialogueRedesignYOffsetShift"].get<int>();
  DIALOGUE_REDESIGN_LINEHEIGHT_SHIFT =
      config["patch"]["dialogueRedesignLineHeightShift"].get<int>();
  HAS_BACKLOG_UNDERLINE = config["gamedef"]["hasBacklogUnderline"].get<bool>();
  if (HAS_BACKLOG_UNDERLINE) {
    BACKLOG_HIGHLIGHT_DEFAULT_HEIGHT =
        config["gamedef"]["backlogHighlightDefaultHeight"].get<int8_t>();
    BACKLOG_HIGHLIGHT_HEIGHT_SHIFT =
        config["patch"]["backlogHighlightHeightShift"].get<int8_t>();
  }
  IMPROVE_DIALOGUE_OUTLINES =
      config["patch"]["improveDialogueOutlines"].get<bool>();
  if (IMPROVE_DIALOGUE_OUTLINES) {
    OUTLINE_PADDING = config["patch"]["outlinePadding"].get<float>();
    OUTLINE_CELL_WIDTH = config["patch"]["outlineCellWidth"].get<uint8_t>();
    OUTLINE_CELL_HEIGHT = config["patch"]["outlineCellHeight"].get<uint8_t>();
    OUTLINE_TEXTURE_ID = config["patch"]["outlineTextureId"].get<uint16_t>();
  }
  GLYPH_ID_FULLWIDTH_SPACE =
      config["gamedef"]["glyphIdFullwidthSpace"].get<uint16_t>();
  GLYPH_ID_HALFWIDTH_SPACE =
      config["gamedef"]["glyphIdHalfwidthSpace"].get<uint16_t>();
  NEEDS_CLEARLIST_TEXT_POSITION_ADJUST =
      config["gamedef"]["needsClearlistTextPositionAdjust"].get<bool>();
  if (config["gamedef"].count("needsCcBacklogNamePosAdjust") == 1) {
    NEEDS_CC_BACKLOG_NAME_POS_ADJUST =
        config["gamedef"]["needsCcBacklogNamePosAdjust"].get<bool>();
  }
  HAS_SPLIT_FONT = config["gamedef"]["hasSplitFont"].get<bool>();
  if (config["patch"].count("tipReimplementation") == 1) {
    TIP_REIMPL = config["patch"]["tipReimplementation"].get<bool>();
  }
  if (TIP_REIMPL) {
    TIP_REIMPL_GLYPH_SIZE =
        config["patch"]["tipReimplementationGlyphSize"].get<int>();
    TIP_REIMPL_LINE_LENGTH =
        config["patch"]["tipReimplementationLineLength"].get<int>();
  }
  if (config["patch"].count("ccBacklogHighlight") == 1) {
    CC_BACKLOG_HIGHLIGHT = config["patch"]["ccBacklogHighlight"].get<bool>();
  }
  if (CC_BACKLOG_HIGHLIGHT) {
    CC_BACKLOG_HIGHLIGHT_HEIGHT_SHIFT =
        config["patch"]["ccBacklogHighlightHeightShift"].get<float>();
    CC_BACKLOG_HIGHLIGHT_SPRITE_HEIGHT =
        config["patch"]["ccBacklogHighlightSpriteHeight"].get<float>();
    CC_BACKLOG_HIGHLIGHT_SPRITE_Y =
        config["patch"]["ccBacklogHighlightSpriteY"].get<float>();
    CC_BACKLOG_HIGHLIGHT_YOFFSET_SHIFT =
        config["patch"]["ccBacklogHighlightYOffsetShift"].get<float>();
  }

  // Script.h
  if (config["patch"].count("customInstGetDicEnabled") == 1) {
    CUSTOM_INST_GETDIC_ENABLED =
        config["patch"]["customInstGetDicEnabled"].get<bool>();
  }
  if (CUSTOM_INST_GETDIC_ENABLED) {
    CUSTOM_INST_GETDIC_GROUP =
        config["patch"]["customInstGetDicGroup"].get<std::string>();
    CUSTOM_INST_GETDIC_OP =
        config["patch"]["customInstGetDicOp"].get<uint8_t>();
  }
}
void LanguageBarrierInit() {
  if (!IsConfigured) {
    IsConfigured = true;

    configInit();
  }

  if (!IsInitialised) {
    if (sigScan("game", "canaryMagesLauncher") != NULL) {
      IsInitialised = true;
      if (config["patch"].count("hijackLauncher") == 1) {
        std::string cmd =
            "start " + config["patch"]["hijackLauncher"].get<std::string>();
        system(cmd.c_str());
        exit(0);
      }
      return;
    }

    if (sigScan("game", "canary") != NULL) {
      // we're past DRM unpacking
      std::remove("languagebarrier\\log.txt");
      // TODO: proper versioning
      LanguageBarrierLog("LanguageBarrier v1.20");
      {
        std::stringstream logstr;
        logstr << "Game: " << configGetGameName();
        LanguageBarrierLog(logstr.str());
      }
      {
        std::stringstream logstr;
        logstr << "Patch: " << configGetPatchName();
        LanguageBarrierLog(logstr.str());
      }
      LanguageBarrierLog("**** Start apprication ****");

      MH_STATUS mhStatus = MH_Initialize();
      if (mhStatus != MH_OK) {
        std::stringstream logstr;
        logstr << "MinHook failed to initialize!"
               << MH_StatusToString(mhStatus);
        LanguageBarrierLog(logstr.str());
        return;
      }

      loadJsonConstants();

      IsInitialised = true;

      gameInit();
    }
  }
}
// TODO: make this better
void LanguageBarrierLog(const std::string &text) {
  std::ofstream logFile("languagebarrier\\log.txt",
                        std::ios_base::out | std::ios_base::app);
  std::time_t t = std::time(NULL);
  logFile << std::put_time(std::gmtime(&t), "[%D %r] ");
  logFile << text << std::endl;
}
bool scanCreateEnableHook(char *category, char *name, uintptr_t *ppTarget,
                          LPVOID pDetour, LPVOID *ppOriginal) {
  *ppTarget = sigScan(category, name);
  if (*ppTarget == NULL) return false;

  MH_STATUS mhStatus;
  mhStatus = MH_CreateHook((LPVOID)*ppTarget, pDetour, ppOriginal);
  if (mhStatus != MH_OK) {
    std::stringstream logstr;
    logstr << "Failed to create hook " << name << ": "
           << MH_StatusToString(mhStatus);
    LanguageBarrierLog(logstr.str());
    return false;
  }
  mhStatus = MH_EnableHook((LPVOID)*ppTarget);
  if (mhStatus != MH_OK) {
    std::stringstream logstr;
    logstr << "Failed to enable hook " << name << ": "
           << MH_StatusToString(mhStatus);
    LanguageBarrierLog(logstr.str());
    return false;
  }

  std::stringstream logstr;
  logstr << "Successfully hooked " << name;
  LanguageBarrierLog(logstr.str());

  return true;
}
bool createEnableApiHook(LPCWSTR pszModule, LPCSTR pszProcName, LPVOID pDetour,
                         LPVOID *ppOriginal) {
  MH_STATUS mhStatus;
  LPVOID pTarget;
  mhStatus =
      MH_CreateHookApiEx(pszModule, pszProcName, pDetour, ppOriginal, &pTarget);
  if (mhStatus != MH_OK) {
    std::stringstream logstr;
    logstr << "Failed to create API hook " << pszModule << "." << pszProcName
           << ": " << MH_StatusToString(mhStatus);
    LanguageBarrierLog(logstr.str());
    return false;
  }
  mhStatus = MH_EnableHook(pTarget);
  if (mhStatus != MH_OK) {
    std::stringstream logstr;
    logstr << "Failed to enable API hook " << pszModule << "." << pszProcName
           << ": " << MH_StatusToString(mhStatus);
    LanguageBarrierLog(logstr.str());
    return false;
  }

  std::stringstream logstr;
  logstr << "Successfully hooked " << pszModule << "." << pszProcName;
  LanguageBarrierLog(logstr.str());

  return true;
}
void slurpFile(const std::string &fileName, std::string **ppBuffer) {
  std::ifstream in(fileName, std::ios::in | std::ios::binary);
  in.seekg(0, std::ios::end);
  *ppBuffer = new std::string(in.tellg(), 0);
  in.seekg(0, std::ios::beg);
  in.read(&((**ppBuffer)[0]), (*ppBuffer)->size());
  in.close();
}
}  // namespace lb
