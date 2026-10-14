#include "MenuLocalization.hpp"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

// Q_INIT_RESOURCE must be invoked from global namespace so the qrc
// initialization symbol resolves on all supported linkers.
static void RegisterMenuLanguageResources()
{
    Q_INIT_RESOURCE(Languages);
}

namespace MphRead::Mods::ClassicMenu
{
    namespace
    {
        // Transparent lookup: no string allocation in the menu render path.
        struct StringHash
        {
            using is_transparent = void;
            std::size_t operator()(std::string_view value) const noexcept
            {
                return std::hash<std::string_view>{}(value);
            }
        };

        struct Translation final
        {
            std::array<std::string, 3> Text;
        };

        using Entries = std::unordered_map<std::string, Translation, StringHash, std::equal_to<>>;
        using EnglishIndex = std::unordered_map<std::string, const Translation*, StringHash, std::equal_to<>>;

        struct MenuCatalog final
        {
            Entries ById;
            EnglishIndex ByEnglish;
            bool Complete = false;
        };

        bool LoadLanguage(const char* filename, QJsonObject& result)
        {
            QFile file(QString::fromLatin1(filename));
            if (!file.open(QIODevice::ReadOnly)) return false;
            QJsonParseError error{};
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject()) return false;
            result = document.object();
            return !result.isEmpty();
        }

        MenuCatalog BuildCatalog()
        {
            RegisterMenuLanguageResources();
            const char* const paths[] = {
                ":/mods/classicmenu/Languages/en.json",
                ":/mods/classicmenu/Languages/ja.json",
                ":/mods/classicmenu/Languages/fr.json"
            };
            QJsonObject languages[3];
            for (int i = 0; i < 3; ++i)
            {
                if (!LoadLanguage(paths[i], languages[i])) return {};
            }

            MenuCatalog result;
            const qsizetype total = languages[0].size();
            if (languages[1].size() != total || languages[2].size() != total) return result;

            result.ById.reserve(static_cast<std::size_t>(total));
            result.ByEnglish.reserve(static_cast<std::size_t>(total));

            for (auto it = languages[0].constBegin(); it != languages[0].constEnd(); ++it)
            {
                const QString& key = it.key();
                // Every language must translate every stable identifier.
                if (!it.value().isString() || !languages[1].value(key).isString()
                    || !languages[2].value(key).isString()) return {};
                Translation entry;
                for (int i = 0; i < 3; ++i)
                {
                    entry.Text[static_cast<std::size_t>(i)] =
                        languages[i].value(key).toString().toUtf8().toStdString();
                    if (entry.Text[static_cast<std::size_t>(i)].empty()) return {};
                }
                const std::string id = key.toUtf8().toStdString();
                result.ById.emplace(id, std::move(entry));
            }

            // Build the ROM compatibility index once. Keeping the stable IDs
            // in the catalog permits changing English copy without changing
            // the Japanese/French keys. Pointer targets are stable because the
            // by-ID map is no longer modified after this initialization.
            for (auto& [id, entry] : result.ById)
            {
                const std::string& english = entry.Text[0];
                if (!result.ByEnglish.emplace(english, &entry).second) return {};
            }
            result.Complete = true;
            return result;
        }

        const MenuCatalog& Catalog()
        {
            static const MenuCatalog catalog = BuildCatalog();
            return catalog;
        }

        const std::string& Display(const Translation& entry, MenuLanguage language)
        {
            const std::size_t index = language == MenuLanguage::Japanese ? 1
                : language == MenuLanguage::French ? 2 : 0;
            return entry.Text[index];
        }
    }

    MenuLanguage ParseMenuLanguage(std::string_view saved) noexcept
    {
        if (saved == "ja" || saved == "Japanese") return MenuLanguage::Japanese;
        if (saved == "fr" || saved == "French") return MenuLanguage::French;
        return MenuLanguage::English;
    }

    const std::string& TranslateMenuKey(std::string_view key, MenuLanguage language)
    {
        static const std::string empty;
        const auto& catalog = Catalog();
        const auto found = catalog.ById.find(key);
        return found == catalog.ById.end() ? empty : Display(found->second, language);
    }

    const std::string& TranslateMenuText(const std::string& english, MenuLanguage language)
    {
        if (language == MenuLanguage::English) return english;
        const auto& catalog = Catalog();
        const auto found = catalog.ByEnglish.find(english);
        return found == catalog.ByEnglish.end() ? english : Display(*found->second, language);
    }

    bool MenuLocalizationCatalogValid()
    {
        return Catalog().Complete;
    }

    std::size_t MenuLocalizationEntryCount()
    {
        return Catalog().ById.size();
    }
}
