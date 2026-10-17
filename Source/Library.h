#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>

// Everything the app knows: plugin files on disk, what the user said about them,
// and how often they show up in projects.

enum class Format { VST3, VST2, CLAP, AU };
juce::String formatName (Format f);

enum class PluginType { Unknown = -1, Effect = 0, Instrument = 1 };

struct PluginFile
{
    juce::File file;
    Format format = Format::VST3;
    bool is32bit = false;
    bool isShell = false;           // Waves WaveShell and friends: many plugins in one file
    juce::String name, vendor;      // best guess from the file (and VST3 moduleinfo)
    PluginType type = PluginType::Unknown;
    juce::StringArray subCategories;
};

struct Plugin
{
    juce::String key;               // normalised name, joins formats into one card
    juce::String name, vendor;
    std::vector<PluginFile> files;

    // what the user set
    bool favorite = false;
    bool hidden = false;            // in the sandbox: never shown in the gallery
    PluginType userType = PluginType::Unknown;
    juce::String userCategory;      // empty = automatic
    juce::String dragFormat;        // empty = use the global priority

    // filled from projects
    int uses = 0, projects = 0;
    juce::Time firstUsed, lastUsed;

    PluginType type() const;
    juce::String category() const;
    juce::String autoCategory() const;
    bool hasFormat (Format f) const;
    const PluginFile* fileForDrag (const juce::StringArray& priority) const;
    bool isShellOnly() const;
    juce::String formatsText() const;
};

// Categories shown in the filter: the built-in ones (stable ids are the Russian labels themselves)
// followed by the ones the user made
juce::StringArray allCategories();
const juce::StringArray& builtInCategories();

struct ProjectRecord
{
    juce::String path;
    juce::int64 modified = 0;
    juce::int64 size = 0;
    std::map<juce::String, int> uses;   // plugin key -> instances
    std::map<juce::String, juce::String> names; // plugin key -> file name seen
};

struct ScanResult
{
    std::vector<PluginFile> files;
    std::vector<ProjectRecord> projects;
    juce::StringArray errors;
};

namespace Scanner
{
    juce::Array<juce::File> defaultPluginFolders();
    std::vector<PluginFile> scanPlugins (const juce::Array<juce::File>& roots, std::function<bool()> shouldExit);
    bool parseProject (const juce::File& f, ProjectRecord& out);
    std::vector<ProjectRecord> scanProjects (const juce::Array<juce::File>& roots,
                                             const std::map<juce::String, ProjectRecord>& cache,
                                             std::function<bool()> shouldExit);

    juce::String cleanName (const juce::String& stem);
    juce::String keyFor (const juce::String& name);
    juce::String keyForFileName (const juce::String& fileName);
    bool isVst2Dll (const juce::File& f, bool& is32bit);
}

class Library
{
public:
    Library();

    juce::File dataDir() const;

    // folders
    juce::Array<juce::File> pluginFolders, projectFolders;
    juce::StringArray formatPriority { "VST3", "CLAP", "VST2", "AU" };
    int staleMonths = 12;
    juce::String themeName { "Asphalt" };
    juce::String lang { "ru" };
    juce::StringArray customCategories;
    float galleryZoom = 1.0f, statsZoom = 1.0f;     // Ctrl + mouse wheel
    bool addCategory (const juce::String& name);
    void removeCategory (const juce::String& name);

    std::vector<Plugin> plugins;
    std::map<juce::String, ProjectRecord> projectCache;
    juce::Time lastScan;

    void applyScan (ScanResult&& r);
    void computeStats();
    Plugin* find (const juce::String& key);

    // per-plugin user data
    void saveSettings();
    void saveUserData();
    void saveProjectCache();
    void loadAll();

    juce::File imageFile (const juce::String& key) const;    // cropped thumbnail
    juce::File originalImageFile (const juce::String& key) const;
    juce::File quarantineDir() const;

    struct ImageState { float zoom = 1.0f, cx = 0.5f, cy = 0.5f; int mode = 0; };
    std::map<juce::String, ImageState> imageStates;

    struct Quarantined { juce::String key, name, from, to; juce::int64 when = 0; };
    std::vector<Quarantined> quarantined;
    void saveQuarantine();

    int projectCount() const { return (int) projectCache.size(); }

private:
    struct UserMeta { bool favorite = false, hidden = false; int type = -1; juce::String category, dragFormat, name; };
    std::map<juce::String, UserMeta> meta;
    void captureMeta();
};
