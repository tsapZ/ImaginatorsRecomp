// Pictures of the toys for the portal menu: the round icons of the game's collection screen.
//
// The chain, all inside the owner's game files (rom/archives), nothing of it in the repository:
//   1. permanent.pak holds one toy data file per toy, ToyData/<Name>_ToyData.igz (IGZ version 9,
//      alchemy/igz_file.h). Its toy objects (CFullCharacterToyData, CVehicleToyData,
//      CBattlePieceToyData, ... every class ending in "ToyData") have the toy id at +0x08, the
//      same as the portal catalog's (Chop Chop 30, Legendary Chop Chop 430, Warnado 2), and at
//      +0x20 a handle to the toy's collection material ("Collection_ChopChop_Normal"). Its
//      CVariantIdentifier objects are the variants: +0x08 deco id, +0x0C year (0 is Spyro's
//      Adventure), +0x10 four flag bytes (LightCore, alternate deco, Series 2, -), +0x14 the
//      variant's name ("Eon's Elite"), +0x1C a handle to its collection material
//      ("Collection_ChopChop_Elite"). A handle is 0x80000000 | an index into the file's external
//      names (EXNM). Together they make the catalog's variant number:
//        variant = year << 12 | Series 2 << 11 | alternate deco << 10 | LightCore << 9 | deco id
//      Read 2026-10-21 in SuperChargersRecomp, whose disc has the same layout; this disc's
//      permanent.pak has the same files (637 in ToyData/).
//   2. ToyCollectionMaterials.pak holds the material instances,
//      materialInstances/ToyCollection/<Material>.igz; the external name in the "image" namespace
//      is the texture ("GuiStandard_diffuse,textures@!ui!Collections!...!Collections_ChopChop`tga,
//      101"), stored in the same archive as textures/<that name>.igz. Its igImage2 is the icon,
//      88x88 DXT5, stored upside down (as Trap Team's portraits; first shown upside down in the
//      menu, seen by the owner 2026-10-21), so it is flipped when it is read.
// 88x88 is all the disc has of most figures (their HUD portraits, ui/HealthHud_2015/Portraits,
// are the same size; the bios' full-screen pictures their toy data names, <Name>_FullScreen_
// materials, are not on the Xbox 360 disc). Some have a sharper copy: the collection tracker's
// CollectibleTrackerIcons.pak holds 128x128 ones (the same pictures) of 49 characters, SuperChargers' and this game's senseis
// (counted 2026-10-21), "textures/ColorMap,textures@!ui!Collections!ToyInventoryIcons!<path>`tga,
// 111.igz" for a material's "GuiStandard_diffuse,...!ToyInventoryIcons!<path>`tga,101", and those
// are taken instead. The menu shows an icon about twice as large as 88 (more at 4K), where the
// GPU's bilinear filter makes it soft. So the first time the menu asks for an icon it is enlarged
// 2x with a Lanczos-3 filter (sharper edges, on premultiplied alpha so the round edge stays clean)
// and kept that way: 120 to 260 KB per icon on screen instead of 70 MB for all of them at start.
// A variant without an entry of its own (most chase variants) shows its toy's default icon.
//
// The menu's traits (SetFigureTraitProvider), as Swap Force's SWAP skills and SuperChargers'
// vehicle terrain (the owner asked for such symbols, 2026-10-21):
//   - A sensei's battle class. LooseData/CYOSClassDataList.igz in JuiceDomain_FrontEnd.pak is the
//     game's class table: one CCYOSClassData per class, +0x28 its character file
//     ("CYOS_Class_Sword"), +0x54 its _senseiList, a CCYOSClassSenseiDataList (+0x08 count, +0x14
//     the item pointers) of CCYOSClassSenseiData with the sensei's toy id at +0x08. Read
//     2026-10-21: ten classes with 31 senseis (Kaos is a class of his own without a symbol; the
//     three NPC classes have none), and the senseis' own file names agree (SwordLifeGuy, SwordArmor
//     under Sword; ArcherGoat, ArcherRobot under Archer; TwinbladesChainsaw under Blades). The
//     symbol is the medallion of the class's picture in the sensei shrine menu, "textures/
//     GuiStandard_diffuse,textures@!ui!CRM!SI_CRM_<Class>Class`tga,101.igz" in CRM.pak (344x308,
//     stored upside down like the others), and the name the game's German text for the class
//     (CYOSClassDataList's text: Ritter, Ninja, Panzerbrecher, ...).
//   - A vehicle's terrain: CVehicleToyData +0x34, 1 land, 2 sky, 3 sea (this game's
//     TemplateLandVehicle / TemplateAirVehicle / TemplateSeaVehicle have 1 / 2 / 3, four bytes
//     later than in SuperChargers; read 2026-10-21 from all 36 vehicles). The symbols are the
//     blueprint screen's, "textures/GuiStandard_diffuse,textures@!ui!Blueprints_2015!
//     <Land|Air|Sea>Vehicle_Icon`tga,101.igz" in E3PressStart.pak, the only archive with all three
//     in colour (172x172; AP_MatchmakingLobby.pak's sea and air ones are two-channel masks).

#include "toy_images.h"

#include "hats.h"
#include "toy_upgrades.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include <rex/logging.h>

#include "game/alchemy/iga_archive.h"
#include "game/alchemy/igz_file.h"
#include "portal_rex/menu_draw.h"
#include "portal_rex/portal_menu.h"

namespace imaginatorsrecomp {
namespace {

using alchemy::IgaArchive;
using alchemy::IgzFile;
using Clock = std::chrono::steady_clock;
using skylanders::FigureImage;

constexpr char kToyDataArchive[] = "permanent.pak";
constexpr char kMaterialArchive[] = "ToyCollectionMaterials.pak";
constexpr std::string_view kToyDataDir = "/ToyData/";
constexpr std::string_view kToyDataSuffix = "_ToyData.igz";
constexpr std::string_view kMaterialDir = "/materialInstances/ToyCollection/";
constexpr std::string_view kTextureDir = "/textures/";
constexpr std::string_view kCollectionPrefix = "Collection_";
constexpr char kImageNamespace[] = "image";
constexpr char kTrackerArchive[] = "CollectibleTrackerIcons.pak";
constexpr std::string_view kIconPath = "textures@!ui!Collections!ToyInventoryIcons!";
constexpr std::string_view kSharpSuffix = "`tga,111.igz";

// The menu's symbols (SetArtImageProvider): the HUD's element badges,
// "textures/GuiStandard_diffuse,textures@!ui!HealthHud_2016!Portraits!Elements!<Element>_HUD`tga,
// 111.igz" in skystonesSmashPortraits.pak (128x128 DXT5, tiled), and the ability shop's coin,
// "...textures@!ui!AbilityShop_2015!coin_icon`tga,101.igz" in JuiceDomain_ActionPacks.pak
// (44x44). Stored upside down like the icons (seen 2026-10-21). This disc has nine of the badges:
// Air's is in none of its archives under any name (searched 2026-10-21; the other element sets,
// the blueprints' grey ones, the loading screen's and the action packs', lack it or Light, Dark or
// Water too), so the menu draws its own Air medallion.
constexpr char kElementArchive[] = "skystonesSmashPortraits.pak";
constexpr std::string_view kElementTexture = "textures@!ui!HealthHud_2016!Portraits!Elements!";
constexpr std::string_view kElementSuffix = "_HUD`tga,111.igz";
constexpr char kCoinArchive[] = "JuiceDomain_ActionPacks.pak";
constexpr std::string_view kCoinTexture = "textures@!ui!AbilityShop_2015!coin_icon`tga,101.igz";
// The menu's element names (portal_menu.h's "element:<name>" keys).
constexpr std::string_view kElements[] = {"magic", "earth", "water", "fire",  "tech",
                                          "undead", "life", "air",  "light", "dark"};

constexpr uint32_t kToyIdField = 0x08;
constexpr uint32_t kToyMaterialField = 0x20;
constexpr uint32_t kDecoField = 0x08;
constexpr uint32_t kYearField = 0x0C;
constexpr uint32_t kFlagsField = 0x10;
constexpr uint32_t kVariantMaterialField = 0x1C;
constexpr uint32_t kVehicleTerrainField = 0x34;  // CVehicleToyData

// The traits' symbols and tables (see the top of this file).
constexpr char kTerrainArchive[] = "E3PressStart.pak";
constexpr std::string_view kTerrainTexture =
    "/textures/GuiStandard_diffuse,textures@!ui!Blueprints_2015!";
constexpr std::string_view kTerrainSuffix = "Vehicle_Icon`tga,101.igz";
constexpr char kClassArchive[] = "CRM.pak";
constexpr std::string_view kClassTexture = "/textures/GuiStandard_diffuse,textures@!ui!CRM!SI_CRM_";
constexpr std::string_view kClassSuffix = "Class`tga,101.igz";
constexpr char kClassListArchive[] = "JuiceDomain_FrontEnd.pak";
constexpr std::string_view kClassListFile = "/LooseData/CYOSClassDataList.igz";
constexpr std::string_view kClassFilePrefix = "CYOS_Class_";
constexpr uint32_t kClassFileField = 0x28;    // CCYOSClassData: its character file's name
constexpr uint32_t kSenseiListField = 0x54;   // CCYOSClassData: its CCYOSClassSenseiDataList
constexpr uint32_t kListCountField = 0x08;
constexpr uint32_t kListDataField = 0x14;
constexpr uint32_t kSenseiToyField = 0x08;    // CCYOSClassSenseiData: the sensei's toy id
constexpr uint32_t kMaxSenseisPerClass = 64;

// The classes by their character file ("CYOS_Class_<file>"), with their picture's name
// ("SI_CRM_<picture>Class", "Scorcerer" as the disc spells it) and the game's German and English
// names.
struct BattleClass {
  std::string_view file;
  std::string_view picture;
  const char* german;
  const char* english;
};
constexpr BattleClass kClasses[] = {
    {"Sword", "Knight", "Ritter", "Knight"},
    {"Thrown", "Ninja", "Ninja", "Ninja"},
    {"Bazooka", "Bazooker", "Panzerbrecher", "Bazooker"},
    {"Pistol", "Quickshot", "Schnellsch\xC3\xBC" "tze", "Quickshot"},
    {"Mage", "Scorcerer", "Zauberer", "Sorcerer"},
    {"Fist", "Brawler", "Boxer", "Brawler"},
    {"Doubler", "Sentinel", "W\xC3\xA4" "chter", "Sentinel"},
    {"Blades", "Swashbuckler", "S\xC3\xA4" "belrassler", "Swashbuckler"},
    {"Archer", "Bowslinger", "Bogenk\xC3\xA4" "mpfer", "Bowslinger"},
    {"Club", "Smasher", "Schmetterer", "Smasher"},
};

constexpr uint16_t kSeries2 = 0x800;
constexpr uint16_t kAlternateDeco = 0x400;
constexpr uint16_t kLightCore = 0x200;
constexpr uint16_t kDecoIdMask = 0x1FF;

struct Toy {
  std::string material;                                   // the default collection material
  std::unordered_map<uint16_t, std::string> variants;     // catalog variant -> material
  std::string character;                                  // "ChopChop" (its toy data file's name)
  uint32_t terrain = 0;                                   // vehicles: 1 land, 2 sky, 3 sea
};

std::mutex g_mutex;
std::unordered_map<uint16_t, Toy> g_toys;                                    // by toy id
std::unordered_map<std::string, std::shared_ptr<const FigureImage>> g_icons;  // by material
std::unordered_map<uint16_t, size_t> g_classes;  // sensei toy id -> kClasses index
bool g_ready = false;

bool EndsWith(const std::string& s, std::string_view suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// The collection material a handle names, or empty.
std::string CollectionMaterial(const IgzFile& igz, uint32_t handle) {
  const auto name = igz.ExternalName(handle);
  if (!name || !name->first.starts_with(kCollectionPrefix)) return {};
  return name->first;
}

uint16_t VariantNumber(const IgzFile& igz, size_t object) {
  const uint32_t flags = igz.Field(object, kFlagsField);
  uint16_t variant = uint16_t((igz.Field(object, kYearField) & 0xF) << 12) |
                     uint16_t(igz.Field(object, kDecoField) & kDecoIdMask);
  if (flags & 0xFF000000) variant |= kLightCore;
  if (flags & 0x00FF0000) variant |= kAlternateDeco;
  if (flags & 0x0000FF00) variant |= kSeries2;
  return variant;
}

// Icons smaller than this are enlarged by kEnlargeFactor when the menu first asks for them.
constexpr int kEnlargeBelow = 256;
constexpr int kEnlargeFactor = 2;

void FlipVertically(FigureImage& image) {
  const size_t row = size_t(image.width) * 4;
  for (int y = 0; y < image.height / 2; ++y) {
    std::swap_ranges(image.rgba.begin() + y * row, image.rgba.begin() + (y + 1) * row,
                     image.rgba.begin() + (image.height - 1 - y) * row);
  }
}

double Lanczos3(double x) {
  constexpr double kPi = 3.14159265358979323846;
  x = std::abs(x);
  if (x < 1e-8) return 1.0;
  if (x >= 3.0) return 0.0;
  const double px = kPi * x;
  return 3.0 * std::sin(px) * std::sin(px / 3.0) / (px * px);
}

// One direction of a separable resample: `count` lines of `in_size` RGBA float pixels, `stride`
// apart in floats (4 for a row, 4 * width for a column), to `out_size` pixels each.
void ResampleLines(const std::vector<float>& in, std::vector<float>& out, int lines, int in_size,
                   int out_size, size_t in_line, size_t in_step, size_t out_line, size_t out_step) {
  const double scale = double(in_size) / out_size;
  for (int o = 0; o < out_size; ++o) {
    const double center = (o + 0.5) * scale - 0.5;
    const int first = int(std::floor(center)) - 2;
    double weights[6], total = 0;
    for (int k = 0; k < 6; ++k) total += weights[k] = Lanczos3(center - (first + k));
    for (int line = 0; line < lines; ++line) {
      float sum[4] = {0, 0, 0, 0};
      for (int k = 0; k < 6; ++k) {
        const int i = std::clamp(first + k, 0, in_size - 1);
        const float* px = &in[line * in_line + i * in_step];
        for (int c = 0; c < 4; ++c) sum[c] += float(weights[k] / total) * px[c];
      }
      float* to = &out[line * out_line + o * out_step];
      for (int c = 0; c < 4; ++c) to[c] = sum[c];
    }
  }
}

std::shared_ptr<const FigureImage> Enlarge(const FigureImage& image, int factor) {
  const int w = image.width, h = image.height, ow = w * factor, oh = h * factor;
  std::vector<float> pre(size_t(w) * h * 4);
  for (size_t i = 0; i < size_t(w) * h; ++i) {
    const float a = image.rgba[4 * i + 3] / 255.0f;
    for (int c = 0; c < 3; ++c) pre[4 * i + c] = image.rgba[4 * i + c] / 255.0f * a;
    pre[4 * i + 3] = a;
  }
  std::vector<float> wide(size_t(ow) * h * 4), tall(size_t(ow) * oh * 4);
  ResampleLines(pre, wide, h, w, ow, size_t(w) * 4, 4, size_t(ow) * 4, 4);       // rows
  ResampleLines(wide, tall, ow, h, oh, 4, size_t(ow) * 4, 4, size_t(ow) * 4);    // columns
  auto out = std::make_shared<FigureImage>();
  out->width = ow;
  out->height = oh;
  out->rgba.resize(tall.size());
  for (size_t i = 0; i < size_t(ow) * oh; ++i) {
    const float a = std::clamp(tall[4 * i + 3], 0.0f, 1.0f);
    for (int c = 0; c < 3; ++c) {
      const float v = a > 1e-4f ? tall[4 * i + c] / a : 0.0f;
      out->rgba[4 * i + c] = uint8_t(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
    }
    out->rgba[4 * i + 3] = uint8_t(std::lround(a * 255.0f));
  }
  return out;
}

using ToyMap = std::unordered_map<uint16_t, Toy>;
using IconMap = std::unordered_map<std::string, std::shared_ptr<const FigureImage>>;

// The first picture of the archive's texture file `index`, upright (stored upside down); nullptr
// if it can't be read.
std::shared_ptr<FigureImage> ReadPicture(const IgaArchive& archive, size_t index) {
  auto pixels = archive.Read(index);
  if (!pixels) return nullptr;
  auto tex = IgzFile::Parse(std::move(*pixels));
  auto rgba = tex ? tex->DecodeFirstImage() : std::nullopt;
  if (!rgba) return nullptr;
  auto image = std::make_shared<FigureImage>();
  image->width = rgba->width;
  image->height = rgba->height;
  image->rgba = std::move(rgba->pixels);
  FlipVertically(*image);
  return image;
}

void IndexToyData(const IgaArchive& archive, ToyMap& toys) {
  for (size_t i = 0; i < archive.size(); ++i) {
    const std::string& name = archive.name(i);
    if (name.find(kToyDataDir) == std::string::npos || !EndsWith(name, kToyDataSuffix)) continue;
    auto data = archive.Read(i);
    if (!data) continue;
    auto igz = IgzFile::Parse(std::move(*data));
    if (!igz) continue;
    // The file's variants belong to its toys; a file with two (a Skylander and its Legendary)
    // gives both the same list, and the catalog's numbers pick the right one.
    std::unordered_map<uint16_t, std::string> variants;
    for (const auto& [object, type] : igz->Objects("CVariantIdentifier")) {
      std::string material = CollectionMaterial(*igz, igz->Field(object, kVariantMaterialField));
      if (!material.empty()) variants.emplace(VariantNumber(*igz, object), std::move(material));
    }
    for (const auto& [object, type] : igz->Objects("ToyData")) {
      const uint32_t id = igz->Field(object, kToyIdField);
      std::string material = CollectionMaterial(*igz, igz->Field(object, kToyMaterialField));
      if (id > 0xFFFF || material.empty()) continue;
      Toy& toy = toys[uint16_t(id)];
      toy.material = std::move(material);
      const size_t file = name.rfind('/') + 1;
      toy.character = name.substr(file, name.size() - file - kToyDataSuffix.size());
      if (type == "CVehicleToyData") toy.terrain = igz->Field(object, kVehicleTerrainField);
      for (const auto& [variant, m] : variants) toy.variants.emplace(variant, m);
    }
  }
}

// The tracker's sharper icons by their path under ToyInventoryIcons ("2015Characters!AirLock").
using SharpMap = std::unordered_map<std::string, size_t>;
SharpMap IndexSharpIcons(const IgaArchive& archive) {
  SharpMap sharp;
  for (size_t i = 0; i < archive.size(); ++i) {
    const std::string& name = archive.name(i);
    const size_t at = name.find(kIconPath);
    if (at == std::string::npos || !EndsWith(name, kSharpSuffix)) continue;
    sharp.emplace(name.substr(at + kIconPath.size(), name.size() - at - kIconPath.size() - kSharpSuffix.size()),
                  i);
  }
  return sharp;
}

// The sharper copy of a material's texture ("GuiStandard_diffuse,textures@!ui!Collections!
// ToyInventoryIcons!2015Characters!AirLock`tga,101"), or nullptr.
std::shared_ptr<FigureImage> SharpIcon(const IgaArchive* tracker, const SharpMap& sharp,
                                       const std::string& texture) {
  const size_t at = texture.find(kIconPath);
  const size_t end = texture.rfind('`');
  if (!tracker || at == std::string::npos || end == std::string::npos || end < at) return nullptr;
  auto it = sharp.find(texture.substr(at + kIconPath.size(), end - at - kIconPath.size()));
  return it == sharp.end() ? nullptr : ReadPicture(*tracker, it->second);
}

void IndexIcons(const IgaArchive& archive, IconMap& icons, const IgaArchive* tracker,
                const SharpMap& sharp) {
  // The textures by file name, then each material's.
  std::unordered_map<std::string, size_t> textures;
  for (size_t i = 0; i < archive.size(); ++i) {
    const std::string& name = archive.name(i);
    const size_t at = name.find(kTextureDir);
    if (at != std::string::npos && EndsWith(name, ".igz")) {
      textures.emplace(name.substr(at + kTextureDir.size(), name.size() - at - kTextureDir.size() - 4),
                       i);
    }
  }
  for (size_t i = 0; i < archive.size(); ++i) {
    const std::string& name = archive.name(i);
    const size_t at = name.find(kMaterialDir);
    if (at == std::string::npos || !EndsWith(name, ".igz")) continue;
    const std::string material =
        name.substr(at + kMaterialDir.size(), name.size() - at - kMaterialDir.size() - 4);
    auto data = archive.Read(i);
    if (!data) continue;
    auto igz = IgzFile::Parse(std::move(*data));
    if (!igz) continue;
    for (uint32_t index = 0; index < 64; ++index) {
      const auto ext = igz->ExternalName(0x80000000u | index);
      if (!ext) break;
      if (ext->second != kImageNamespace) continue;
      auto texture = textures.find(ext->first);
      if (texture == textures.end()) break;
      auto image = SharpIcon(tracker, sharp, ext->first);
      if (!image) image = ReadPicture(archive, texture->second);
      if (image) icons[material] = std::move(image);
      break;
    }
  }
}

// Each sensei's class (kClasses index) by toy id, from the game's class table.
std::unordered_map<uint16_t, size_t> IndexClasses(const IgaArchive& archive) {
  std::unordered_map<uint16_t, size_t> classes;
  for (size_t i = 0; i < archive.size(); ++i) {
    if (!EndsWith(archive.name(i), kClassListFile)) continue;
    auto data = archive.Read(i);
    auto igz = data ? IgzFile::Parse(std::move(*data)) : std::nullopt;
    if (!igz) break;
    for (const auto& [object, type] : igz->Objects("CCYOSClassData")) {
      const std::string file = igz->StringAt(igz->Field(object, kClassFileField));
      if (!file.starts_with(kClassFilePrefix)) continue;
      const std::string_view kind = std::string_view(file).substr(kClassFilePrefix.size());
      const auto known = std::find_if(std::begin(kClasses), std::end(kClasses),
                                      [&](const BattleClass& c) { return c.file == kind; });
      const uint32_t list_pointer = igz->Field(object, kSenseiListField);
      const auto list = list_pointer ? igz->Pointer(list_pointer) : std::nullopt;
      if (known == std::end(kClasses) || !list) continue;
      const uint32_t count = igz->Field(*list, kListCountField);
      const uint32_t items_pointer = igz->Field(*list, kListDataField);
      const auto items = items_pointer ? igz->Pointer(items_pointer) : std::nullopt;
      if (!items || count > kMaxSenseisPerClass) continue;
      for (uint32_t k = 0; k < count; ++k) {
        const uint32_t sensei_pointer = igz->Field(*items, 4 * k);
        const auto sensei = sensei_pointer ? igz->Pointer(sensei_pointer) : std::nullopt;
        if (!sensei) continue;
        const uint32_t id = igz->Field(*sensei, kSenseiToyField);
        if (id <= 0xFFFF) classes[uint16_t(id)] = size_t(known - std::begin(kClasses));
      }
    }
    break;
  }
  return classes;
}

// The medallion out of a class picture (upright, 344x308): the gold ring is 196 pixels across
// around (172, 125) in all ten (measured 2026-10-21); the shrine's background around it is made
// transparent so the menu shows only the round symbol.
std::shared_ptr<FigureImage> Medallion(const FigureImage& picture) {
  constexpr int kSize = 200, kLeft = 72, kTop = 25;
  constexpr double kRadius = 99.5;
  if (picture.width < kLeft + kSize || picture.height < kTop + kSize) return nullptr;
  auto out = std::make_shared<FigureImage>();
  out->width = kSize;
  out->height = kSize;
  out->rgba.resize(size_t(kSize) * kSize * 4);
  for (int y = 0; y < kSize; ++y) {
    for (int x = 0; x < kSize; ++x) {
      const uint8_t* from = &picture.rgba[(size_t(kTop + y) * picture.width + kLeft + x) * 4];
      uint8_t* to = &out->rgba[(size_t(y) * kSize + x) * 4];
      std::copy(from, from + 4, to);
      const double d = std::hypot(x + 0.5 - kSize / 2.0, y + 0.5 - kSize / 2.0);
      to[3] = uint8_t(std::lround(to[3] * std::clamp(kRadius - d, 0.0, 1.0)));
    }
  }
  return out;
}

// The menu's symbols in `archive`: the element badges (kElementTexture + <Element> +
// kElementSuffix) as "element:<element>", the coin (kCoinTexture) as "ui:gold", the terrain
// symbols as "vehicle:land" / "vehicle:sky" / "vehicle:sea" and the class medallions as
// "class:<picture>". Returns how many it read.
int IndexArt(const IgaArchive& archive, IconMap& icons) {
  int count = 0;
  for (size_t i = 0; i < archive.size(); ++i) {
    const std::string& name = archive.name(i);
    std::string key;
    bool medallion = false;
    if (EndsWith(name, kCoinTexture)) {
      key = "ui:gold";
    } else if (const size_t at = name.find(kTerrainTexture);
               at != std::string::npos && EndsWith(name, kTerrainSuffix)) {
      const std::string kind = name.substr(at + kTerrainTexture.size(),
                                           name.size() - at - kTerrainTexture.size() - kTerrainSuffix.size());
      if (kind == "Land") key = "vehicle:land";
      else if (kind == "Air") key = "vehicle:sky";
      else if (kind == "Sea") key = "vehicle:sea";
      else continue;
    } else if (const size_t at = name.find(kClassTexture);
               at != std::string::npos && EndsWith(name, kClassSuffix)) {
      key = "class:" + name.substr(at + kClassTexture.size(),
                                   name.size() - at - kClassTexture.size() - kClassSuffix.size());
      medallion = true;
    } else if (const size_t at = name.find(kElementTexture);
               at != std::string::npos && EndsWith(name, kElementSuffix)) {
      std::string element = name.substr(at + kElementTexture.size(),
                                        name.size() - at - kElementTexture.size() - kElementSuffix.size());
      std::transform(element.begin(), element.end(), element.begin(),
                     [](unsigned char c) { return char(std::tolower(c)); });
      if (std::find(std::begin(kElements), std::end(kElements), element) == std::end(kElements)) continue;
      key = "element:" + element;
    } else {
      continue;
    }
    if (icons.contains(key)) continue;
    std::shared_ptr<const FigureImage> image = ReadPicture(archive, i);
    if (image && medallion) image = Medallion(*image);
    if (image) {
      icons[key] = std::move(image);
      ++count;
    }
  }
  return count;
}

void Load(std::filesystem::path rom) {
  const auto start = Clock::now();
  auto toydata = IgaArchive::Open(rom / "archives" / kToyDataArchive);
  auto materials = IgaArchive::Open(rom / "archives" / kMaterialArchive);
  if (!toydata || !materials) {
    REXLOG_WARN("Toy images: {} or {} not readable", kToyDataArchive, kMaterialArchive);
    return;
  }
  ToyMap toys;
  IconMap icons;
  IndexToyData(*toydata, toys);
  auto tracker = IgaArchive::Open(rom / "archives" / kTrackerArchive);
  const SharpMap sharp = tracker ? IndexSharpIcons(*tracker) : SharpMap{};
  IndexIcons(*materials, icons, tracker ? &*tracker : nullptr, sharp);
  int symbols = 0;
  for (const char* name : {kElementArchive, kCoinArchive, kTerrainArchive, kClassArchive}) {
    if (auto art = IgaArchive::Open(rom / "archives" / name)) symbols += IndexArt(*art, icons);
  }
  std::unordered_map<uint16_t, size_t> classes;
  if (auto list = IgaArchive::Open(rom / "archives" / kClassListArchive)) classes = IndexClasses(*list);
  const size_t class_count = classes.size();
  size_t variants = 0;
  for (const auto& [id, toy] : toys) variants += toy.variants.size();
  const size_t toy_count = toys.size(), icon_count = icons.size();
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_toys = std::move(toys);
    g_icons = std::move(icons);
    g_classes = std::move(classes);
    g_ready = true;
  }
  REXLOG_INFO("Toy images: {} toys with {} variants, {} icons and {} symbols, {} senseis' classes, "
              "in {} ms", toy_count, variants, icon_count - symbols, symbols, class_count,
              std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count());
}

// The first time an icon is asked for, it is enlarged (see the top of this file). g_mutex held.
std::shared_ptr<const FigureImage> Icon(const std::string& material) {
  auto it = g_icons.find(material);
  if (it == g_icons.end()) return nullptr;
  if (it->second->width < kEnlargeBelow) it->second = Enlarge(*it->second, kEnlargeFactor);
  return it->second;
}

// A sensei's class or a vehicle's terrain as the menu's trait, or nothing for the other toys.
std::optional<skylanders::FigureTrait> Trait(uint16_t id) {
  uint32_t terrain = 0;
  std::optional<size_t> battle_class;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_ready) return std::nullopt;
    if (auto it = g_toys.find(id); it != g_toys.end()) terrain = it->second.terrain;
    if (auto it = g_classes.find(id); it != g_classes.end()) battle_class = it->second;
  }
  const bool german = skylanders::menu_draw::GermanText();
  if (battle_class) {
    const BattleClass& c = kClasses[*battle_class];
    return skylanders::FigureTrait{"class:" + std::string(c.picture), german ? c.german : c.english};
  }
  switch (terrain) {
    case 1: return skylanders::FigureTrait{"vehicle:land", german ? "Landfahrzeug" : "Land Vehicle"};
    case 2: return skylanders::FigureTrait{"vehicle:sky", german ? "Himmelsfahrzeug" : "Sky Vehicle"};
    case 3: return skylanders::FigureTrait{"vehicle:sea", german ? "Seefahrzeug" : "Sea Vehicle"};
    default: return std::nullopt;
  }
}

std::shared_ptr<const FigureImage> Lookup(uint16_t id, uint16_t variant) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!g_ready) return nullptr;
  auto it = g_toys.find(id);
  if (it == g_toys.end()) return nullptr;
  const Toy& toy = it->second;
  if (auto v = toy.variants.find(variant); v != toy.variants.end()) {
    if (auto icon = Icon(v->second)) return icon;
  }
  return Icon(toy.material);
}

}  // namespace

std::string ToyCharacter(uint16_t id) {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_toys.find(id);
  return it == g_toys.end() ? std::string() : it->second.character;
}

void InstallToyImages(const std::filesystem::path& rom_dir) {
  std::thread(Load, rom_dir).detach();
  skylanders::SetFigureImageProvider(
      [](uint16_t id, uint16_t variant) { return Lookup(id, variant); });
  skylanders::SetFigureTraitProvider([](uint16_t id, uint16_t) { return Trait(id); });
  skylanders::SetArtImageProvider([](const std::string& key) -> std::shared_ptr<const FigureImage> {
    if (auto hat = HatPicture(key)) return hat;  // "hat:<number>" (hats.cpp)
    if (key.starts_with("upgrade:")) return UpgradeImage(key);  // toy_upgrades.cpp
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_ready ? Icon(key) : nullptr;
  });
}

}  // namespace imaginatorsrecomp
