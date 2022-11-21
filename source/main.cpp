/**
 * Copyright (C) 2019 - 2020 WerWolv
 * 
 * This file is part of EdiZon
 * 
 * EdiZon is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * EdiZon is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with EdiZon.  If not, see <http://www.gnu.org/licenses/>.
 */

#define TESLA_INIT_IMPL
#include <tesla.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <switch.h>
#include <filesystem>

#include <switch/nro.h>
#include <switch/nacp.h>

#include <edizon.h>
#include <helpers/util.h>
#include "utils.hpp"
#include "cheat.hpp"

#include <unistd.h>
#include <netinet/in.h>


class GuiCheats : public tsl::Gui {
public:
    GuiCheats(std::string section) { 
        this->m_section = section;
    }
    ~GuiCheats() { }

    virtual tsl::elm::Element* createUI() override {
        this->m_cheats = edz::cheat::CheatManager::getCheats();
        auto rootFrame = new tsl::elm::OverlayFrame("EdiZon Modded by ELY M.", "Cheats");

        if (m_cheats.size() == 0) {
            auto warning = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, u16 x, u16 y, u16 w, u16 h){
                renderer->drawString("\uE150", false, 180, 250, 90, renderer->a(0xFFFF));
                renderer->drawString("No Cheats loaded!", false, 110, 340, 25, renderer->a(0xFFFF));
            });

            rootFrame->setContent(warning);

        } else {
            auto list = new tsl::elm::List();
            std::string head = "Section: " + this->m_section;

            if(m_section.length() > 0) list->addItem(new tsl::elm::CategoryHeader(head));
            else list->addItem(new tsl::elm::CategoryHeader("Available cheats"));

            bool skip = false, inSection = false;
            std::string skipUntil = "";

            for (auto &cheat : this->m_cheats) {
                // Find section start and end
                if(this->m_section.length() > 0 && !inSection && cheat->getName().find("--SectionStart:" + this->m_section + "--") == std::string::npos) continue;
                else if(cheat->getName().find("--SectionStart:" + this->m_section + "--") != std::string::npos) { inSection = true; continue; }
                else if(inSection && cheat->getName().find("--SectionEnd:" + this->m_section + "--") != std::string::npos) break;

                // new section
                if(!skip && cheat->getName().find("--SectionStart:") != std::string::npos){

                    //remove formatting
                    std::string name = cheat->getName();
                    replaceAll(name, "--", "");
                    replaceAll(name, "SectionStart:", "");

                    //create submenu button
                    auto cheatsSubmenu = new tsl::elm::ListItem(name);
                    cheatsSubmenu->setClickListener([name = name](s64 keys) {
                        if (keys & HidNpadButton_A) {
                            tsl::changeTo<GuiCheats>(name);
                            return true;
                        }
                        return false;
                    });
                    list->addItem(cheatsSubmenu);
                    this->m_numCheats++;

                    //skip over items in section
                    skip = true;
                    skipUntil = "--SectionEnd:" + name + "--";
                }
                // found end of child section
                else if (skip && cheat->getName().compare(skipUntil) == 0){
                    skip = false;
                    skipUntil = "";
                }
                // items to add to section
                else if(!skip && (inSection || this->m_section.length() < 1)) {
                    auto cheatToggleItem = new tsl::elm::ToggleListItem(cheat->getName(), cheat->isEnabled());
                    cheatToggleItem->setStateChangedListener([&cheat](bool state) { cheat->setState(state); });

                    this->m_cheatToggleItems.insert({cheat, cheatToggleItem});
                    list->addItem(cheatToggleItem);
                    this->m_numCheats++;
                }
            }

            // display if no cheats in submenu
            if(this->m_numCheats < 1){
                auto warning = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, u16 x, u16 y, u16 w, u16 h){
                    renderer->drawString("\uE150", false, 180, 250, 90, renderer->a(0xFFFF));
                    renderer->drawString("No Cheats in Submenu!", false, 110, 340, 25, renderer->a(0xFFFF));
                });

                rootFrame->setContent(warning);
            } else rootFrame->setContent(list);
        }

        return rootFrame;
    }

    int FindSectionStart(){
        for(u32 i = 0; i < this->m_cheats.size(); i++)
            if(this->m_cheats[i]->getName().find("--SectionStart:" + m_section + "--") != std::string::npos) return i + 1;
        return 0;
    }

    int FindSectionEnd(){
        for(u32 i = 0; i < this->m_cheats.size(); i++)
            if(this->m_cheats[i]->getName().find("--SectionEnd:" + m_section + "--") != std::string::npos) return i;
        return this->m_cheats.size();
    }

    void replaceAll(std::string& str, const std::string& from, const std::string& to) {
        if(from.empty())
            return;
        size_t start_pos = 0;
        while((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
    }

    virtual void update() {
        for (auto const& [Cheat, Button] : this->m_cheatToggleItems)
            Button->setState(Cheat->isEnabled());
    }

private:
    int m_numCheats = 0;
    std::string m_section;
    std::vector<edz::cheat::Cheat*> m_cheats;
    std::map<edz::cheat::Cheat*, tsl::elm::ToggleListItem*> m_cheatToggleItems;
};

class GuiMain : public tsl::Gui {
public:
    GuiMain() { }

    ~GuiMain() { }

    virtual tsl::elm::Element* createUI() {
        auto *rootFrame = new tsl::elm::HeaderOverlayFrame();
        rootFrame->setHeader(new tsl::elm::CustomDrawer([this](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("EdiZon - Modded by ELY M.", false, 20, 50, 30, renderer->a(tsl::style::color::ColorText));
            renderer->drawString("v1.0.3", false, 20, 70, 20, renderer->a(tsl::style::color::ColorDescription));

            if (edz::cheat::CheatManager::getProcessID() != 0) {
                renderer->drawString("Program ID:", false, 50, 100, 20, renderer->a(tsl::style::color::ColorText));
                renderer->drawString("Build ID:", false, 50, 120, 20, renderer->a(tsl::style::color::ColorText));
                renderer->drawString("Process ID:", false, 50, 140, 20, renderer->a(tsl::style::color::ColorText));
                renderer->drawString(GuiMain::s_runningTitleIDString.c_str(), false, 200, 100, 20, renderer->a(tsl::style::color::ColorHighlight));
                renderer->drawString(GuiMain::s_runningBuildIDString.c_str(), false, 200, 120, 20, renderer->a(tsl::style::color::ColorHighlight));
                renderer->drawString(GuiMain::s_runningProcessIDString.c_str(), false, 200, 140, 20, renderer->a(tsl::style::color::ColorHighlight));
            }
        }));

        auto list = new tsl::elm::List();

        auto cheatsItem = new tsl::elm::ListItem("Cheats");
        cheatsItem->setClickListener([](s64 keys) {
            if (keys & HidNpadButton_A) {
                tsl::changeTo<GuiCheats>("");
                return true;
            }

            return false;
        });

        list->addItem(cheatsItem);
        rootFrame->setContent(list);

        return rootFrame;
    }

    virtual void update() { }

public:
    static inline std::string s_runningTitleIDString;
    static inline std::string s_runningProcessIDString;
    static inline std::string s_runningBuildIDString;
};

class EdiZonOverlay : public tsl::Overlay {
public:
    EdiZonOverlay() { }
    ~EdiZonOverlay() { }

    void initServices() override {
        dmntchtInitialize();
        edz::cheat::CheatManager::initialize();

    } 

    virtual void exitServices() override {
        dmntchtExit();
        edz::cheat::CheatManager::exit();

    }

    virtual void onShow() override {
        edz::cheat::CheatManager::reload();
        GuiMain::s_runningTitleIDString     = formatString("%016lX", edz::cheat::CheatManager::getTitleID());
        GuiMain::s_runningBuildIDString     = formatString("%016lX", edz::cheat::CheatManager::getBuildID());
        GuiMain::s_runningProcessIDString   = formatString("%lu", edz::cheat::CheatManager::getProcessID());
    }

    std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<GuiMain>();
    }

    
};


int main(int argc, char **argv) {
    return tsl::loop<EdiZonOverlay>(argc, argv);
}