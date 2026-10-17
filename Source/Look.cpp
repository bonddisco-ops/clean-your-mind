#include "Ui.h"

using namespace juce;

static bool gRussian = true;
void setUiLanguage (const String& lang) { gRussian = lang != "en"; }
bool uiIsRussian() { return gRussian; }
String tr (const char* ru, const char* en) { return String::fromUTF8 (gRussian ? ru : en); }

Font uiFont (float height, bool bold, float kerning)
{
    Font f (FontOptions (height, bold ? Font::bold : Font::plain));
    if (kerning != 0.0f)
        f.setExtraKerningFactor (kerning);
    return f;
}

Pal Pal::from (const Theme& t)
{
    Pal p;
    p.bg = t.bg;
    p.ink = t.ink;
    p.mute = t.mute;
    p.accent = t.accent;
    p.accentHi = t.accentHi;
    p.accentDim = t.accentDim;
    if (t.lightBg)
    {
        p.panel = t.bg.darker (0.10f);
        p.widget = t.bg.darker (0.18f);
        p.widgetHi = t.bg.darker (0.28f);
        p.stroke = Colours::black.withAlpha (0.14f);
        p.faint = t.ink.withAlpha (0.35f);
        p.onAccent = Colours::white;
    }
    else
    {
        p.panel = t.bg.darker (0.45f);
        p.widget = t.bg.brighter (0.10f).interpolatedWith (Colour (0xff1f2024), 0.5f);
        p.widgetHi = p.widget.brighter (0.25f);
        p.stroke = Colours::white.withAlpha (0.07f);
        p.faint = t.ink.withAlpha (0.28f);
        p.onAccent = t.accent.getPerceivedBrightness() > 0.55f ? Colour (0xff1a0d14) : Colours::white;
    }
    return p;
}

//==============================================================================
CymLook::CymLook() { setTheme (Theme::all().front()); }

void CymLook::setTheme (const Theme& t)
{
    current = &t;
    p = Pal::from (t);
    setColour (ResizableWindow::backgroundColourId, t.bg);
    setColour (DocumentWindow::backgroundColourId, t.bg);
    setColour (Label::textColourId, t.ink);
    setColour (TextEditor::backgroundColourId, p.panel);
    setColour (TextEditor::textColourId, t.ink);
    setColour (TextEditor::highlightColourId, t.accent.withAlpha (0.35f));
    setColour (TextEditor::highlightedTextColourId, t.ink);
    setColour (TextEditor::focusedOutlineColourId, t.accent);
    setColour (TextEditor::outlineColourId, p.stroke);
    setColour (CaretComponent::caretColourId, t.accent);
    setColour (ComboBox::backgroundColourId, p.widget);
    setColour (ComboBox::textColourId, t.ink);
    setColour (ComboBox::outlineColourId, p.stroke);
    setColour (ComboBox::arrowColourId, t.accent);
    setColour (PopupMenu::backgroundColourId, p.widget.withAlpha (1.0f));
    setColour (PopupMenu::textColourId, t.ink);
    setColour (PopupMenu::headerTextColourId, t.accent);
    setColour (PopupMenu::highlightedBackgroundColourId, t.accent);
    setColour (PopupMenu::highlightedTextColourId, p.onAccent);
    setColour (TextButton::buttonColourId, p.widget);
    setColour (TextButton::buttonOnColourId, t.accent);
    setColour (TextButton::textColourOffId, t.ink);
    setColour (TextButton::textColourOnId, p.onAccent);
    setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    setColour (ListBox::outlineColourId, Colours::transparentBlack);
    setColour (ScrollBar::thumbColourId, t.accentDim);
    setColour (Slider::thumbColourId, t.accent);
    setColour (Slider::trackColourId, t.accent);
    setColour (Slider::backgroundColourId, p.panel);
    setColour (AlertWindow::backgroundColourId, p.widget.withAlpha (1.0f));
    setColour (AlertWindow::textColourId, t.ink);
    setColour (AlertWindow::outlineColourId, t.accent);
    setColour (TooltipWindow::textColourId, t.ink);
}

const Pal& palOf (const Component& c)
{
    if (auto* l = dynamic_cast<CymLook*> (&c.getLookAndFeel()))
        return l->pal();
    static const Pal fallback = Pal::from (Theme::all().front());
    return fallback;
}

const Theme& themeOf (const Component& c)
{
    if (auto* l = dynamic_cast<CymLook*> (&c.getLookAndFeel()))
        return l->theme();
    return Theme::all().front();
}

void CymLook::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool pill = (bool) b.getProperties().getWithDefault ("pill", true);
    const bool primary = (bool) b.getProperties().getWithDefault ("primary", false);
    const float rad = pill ? r.getHeight() * 0.5f : 8.0f;
    const bool on = b.getToggleState() || primary;
    auto fillC = on ? p.accent : p.widget;
    if (highlighted && b.isEnabled()) fillC = on ? p.accentHi : p.widgetHi;
    if (down) fillC = fillC.darker (0.1f);
    if (! b.isEnabled()) fillC = fillC.withMultipliedAlpha (0.5f);
    g.setColour (fillC);
    g.fillRoundedRectangle (r, rad);
    if (! on)
    {
        const bool outlineAccent = (bool) b.getProperties().getWithDefault ("accentOutline", false);
        g.setColour (outlineAccent ? p.accent.withAlpha (0.5f) : p.stroke.withMultipliedAlpha (1.3f));
        g.drawRoundedRectangle (r, rad, 1.0f);
    }
}

void CymLook::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const bool on = b.getToggleState() || (bool) b.getProperties().getWithDefault ("primary", false);
    const bool outlineAccent = (bool) b.getProperties().getWithDefault ("accentOutline", false);
    g.setColour (on ? p.onAccent : (outlineAccent ? p.accentHi : p.ink).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));
    g.setFont (getTextButtonFont (b, b.getHeight()).boldened().withStyle (on ? Font::bold : Font::plain));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (10, 0), Justification::centred, 1);
}

Font CymLook::getTextButtonFont (TextButton& b, int h)
{
    const float size = (float) b.getProperties().getWithDefault ("fontSize", jmin (13.0f, (float) h * 0.4f));
    return uiFont (size);
}

void CymLook::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    const auto r = Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (p.widget);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (box.hasKeyboardFocus (true) ? p.accent.withAlpha (0.6f) : p.stroke.withMultipliedAlpha (1.3f));
    g.drawRoundedRectangle (r, 8.0f, 1.0f);
    Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 4.5f, ay - 2.5f);
    arrow.lineTo (ax, ay + 2.5f);
    arrow.lineTo (ax + 4.5f, ay - 2.5f);
    g.setColour (p.accent);
    g.strokePath (arrow, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
}

void CymLook::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

Font CymLook::getComboBoxFont (ComboBox&) { return uiFont (13.0f); }
Font CymLook::getPopupMenuFont() { return uiFont (14.0f); }

void CymLook::drawScrollbar (Graphics& g, ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart,
                             int thumbSize, bool over, bool down)
{
    Rectangle<float> thumb = vertical ? Rectangle<float> ((float) x + 2.0f, (float) thumbStart, (float) w - 4.0f, (float) thumbSize)
                                      : Rectangle<float> ((float) thumbStart, (float) y + 2.0f, (float) thumbSize, (float) h - 4.0f);
    g.setColour ((over || down ? p.accent : p.accentDim).withAlpha (0.8f));
    g.fillRoundedRectangle (thumb.reduced (vertical ? 1.0f : 0.0f, vertical ? 0.0f : 1.0f), 3.0f);
}

void CymLook::fillTextEditorBackground (Graphics& g, int w, int h, TextEditor&)
{
    g.setColour (p.panel);
    g.fillRoundedRectangle (Rectangle<int> (w, h).toFloat(), 9.0f);
}

void CymLook::drawTextEditorOutline (Graphics& g, int w, int h, TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? p.accent.withAlpha (0.7f) : p.stroke.withMultipliedAlpha (1.3f));
    g.drawRoundedRectangle (Rectangle<int> (w, h).toFloat().reduced (0.5f), 9.0f, 1.0f);
}

Rectangle<int> CymLook::getTooltipBounds (const String& tip, Point<int> pos, Rectangle<int> parent)
{
    AttributedString s;
    s.append (tip, uiFont (13.0f), p.ink);
    TextLayout tl;
    tl.createLayout (s, 320.0f);
    const int w = (int) std::ceil (tl.getWidth()) + 22, h = (int) std::ceil (tl.getHeight()) + 16;
    const int x = pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18;
    const int y = pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 18;
    return Rectangle<int> (x, y, w, h).constrainedWithin (parent);
}

void CymLook::drawTooltip (Graphics& g, const String& text, int w, int h)
{
    const auto r = Rectangle<int> (w, h).toFloat();
    g.setColour (p.panel.withAlpha (0.97f));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (p.accent.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
    AttributedString s;
    s.append (text, uiFont (13.0f), p.ink);
    TextLayout tl;
    tl.createLayout (s, (float) w - 22.0f);
    tl.draw (g, r.reduced (11.0f, 8.0f));
}

void CymLook::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                Slider::SliderStyle, Slider&)
{
    const float cy = (float) y + (float) h * 0.5f;
    const Rectangle<float> track ((float) x, cy - 3.0f, (float) w, 6.0f);
    g.setColour (p.panel);
    g.fillRoundedRectangle (track, 3.0f);
    g.setColour (p.accent);
    g.fillRoundedRectangle (track.withRight (pos), 3.0f);
    g.setColour (p.accentHi);
    g.fillEllipse (pos - 8.0f, cy - 8.0f, 16.0f, 16.0f);
    g.setColour (p.bg);
    g.fillEllipse (pos - 3.0f, cy - 3.0f, 6.0f, 6.0f);
}

//==============================================================================
Chip::Chip (const String& text, bool pill) : TextButton (text)
{
    getProperties().set ("pill", pill);
    setWantsKeyboardFocus (false);
    setMouseCursor (MouseCursor::PointingHandCursor);
}

Path iconPath (Icon i)
{
    // 24x24 stroke icons
    Path p;
    switch (i)
    {
        case Icon::Refresh:
            p.addCentredArc (12, 12, 8, 8, 0, MathConstants<float>::pi * 0.15f, MathConstants<float>::pi * 1.75f, true);
            p.startNewSubPath (20, 3.5f); p.lineTo (20, 9); p.lineTo (14.5f, 9);
            break;
        case Icon::Theme:
            p.addEllipse (8, 8, 8, 8);
            for (int k = 0; k < 8; ++k)
            {
                const float a = (float) k * MathConstants<float>::pi * 0.25f;
                p.startNewSubPath (12 + std::cos (a) * 7.0f, 12 + std::sin (a) * 7.0f);
                p.lineTo (12 + std::cos (a) * 9.5f, 12 + std::sin (a) * 9.5f);
            }
            break;
        case Icon::Settings:
            p.startNewSubPath (4, 6); p.lineTo (14, 6); p.startNewSubPath (18, 6); p.lineTo (20, 6);
            p.startNewSubPath (4, 12); p.lineTo (8, 12); p.startNewSubPath (12, 12); p.lineTo (20, 12);
            p.startNewSubPath (4, 18); p.lineTo (16, 18);
            p.addEllipse (14, 4, 4, 4); p.addEllipse (8, 10, 4, 4); p.addEllipse (16, 16, 4, 4);
            break;
        case Icon::Folder:
            p.startNewSubPath (3, 7); p.lineTo (9, 7); p.lineTo (11, 9); p.lineTo (21, 9); p.lineTo (21, 19); p.lineTo (3, 19); p.closeSubPath();
            break;
        case Icon::Plus:
            p.startNewSubPath (12, 5); p.lineTo (12, 19); p.startNewSubPath (5, 12); p.lineTo (19, 12);
            break;
        case Icon::Cross:
            p.startNewSubPath (6, 6); p.lineTo (18, 18); p.startNewSubPath (18, 6); p.lineTo (6, 18);
            break;
        case Icon::Star:
            p.addStar ({ 12, 12.5f }, 5, 4.2f, 9.5f, 0.0f);
            break;
        case Icon::Camera:
            p.addRoundedRectangle (3.0f, 6.0f, 18.0f, 13.0f, 2.0f);
            p.addEllipse (9, 9.5f, 6, 6);
            p.startNewSubPath (8, 6); p.lineTo (9.5f, 3.5f); p.lineTo (14.5f, 3.5f); p.lineTo (16, 6);
            break;
        case Icon::Back:
            p.startNewSubPath (15, 5); p.lineTo (8, 12); p.lineTo (15, 19);
            break;
        case Icon::Search:
            p.addEllipse (4, 4, 14, 14); p.startNewSubPath (16.5f, 16.5f); p.lineTo (21, 21);
            break;
        case Icon::Reveal:
            p.startNewSubPath (3, 7); p.lineTo (9, 7); p.lineTo (11, 9); p.lineTo (21, 9); p.lineTo (21, 19); p.lineTo (3, 19); p.closeSubPath();
            p.startNewSubPath (10, 14); p.lineTo (15, 14); p.startNewSubPath (13, 12); p.lineTo (15, 14); p.lineTo (13, 16);
            break;
        case Icon::Box:
            p.addRoundedRectangle (3.0f, 4.0f, 18.0f, 5.0f, 1.0f);
            p.startNewSubPath (5, 9); p.lineTo (5, 20); p.lineTo (19, 20); p.lineTo (19, 9);
            p.startNewSubPath (10, 13); p.lineTo (14, 13);
            break;
        case Icon::Ban:
            p.addEllipse (3.5f, 3.5f, 17, 17);
            p.startNewSubPath (6.2f, 17.8f); p.lineTo (17.8f, 6.2f);
            break;
        case Icon::Globe:
            p.addEllipse (3, 3, 18, 18);
            p.startNewSubPath (3, 12); p.lineTo (21, 12);
            p.addCentredArc (12, 12, 4.5f, 9, 0, 0, MathConstants<float>::twoPi, true);
            break;
    }
    return p;
}

IconButton::IconButton (Icon i, const String& label) : Button (label), icon (i)
{
    setWantsKeyboardFocus (false);
    setMouseCursor (MouseCursor::PointingHandCursor);
}

int IconButton::idealWidth() const
{
    return getButtonText().isEmpty() ? 36 : 56 + GlyphArrangement::getStringWidthInt (uiFont (13.0f, true), getButtonText());
}

void IconButton::paintButton (Graphics& g, bool highlighted, bool down)
{
    const auto& p = palOf (*this);
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    auto fillC = primary ? p.accent : p.widget;
    if (highlighted) fillC = primary ? p.accentHi : p.widgetHi;
    if (down) fillC = fillC.darker (0.1f);
    g.setColour (fillC.withMultipliedAlpha (isEnabled() ? 1.0f : 0.5f));
    g.fillRoundedRectangle (r, 8.0f);
    if (! primary)
    {
        g.setColour (p.stroke.withMultipliedAlpha (1.3f));
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
    }
    const float s = 16.0f;
    const bool hasText = getButtonText().isNotEmpty();
    const auto iconBox = hasText ? Rectangle<float> (r.getX() + 13.0f, r.getCentreY() - s * 0.5f, s, s)
                                 : Rectangle<float> (s, s).withCentre (r.getCentre());
    auto path = iconPath (icon);
    path.applyTransform (AffineTransform::scale (s / 24.0f).translated (iconBox.getX(), iconBox.getY()));
    const auto iconC = primary ? p.onAccent : (accentIcon ? p.accent : p.ink);
    g.setColour (iconC.withMultipliedAlpha (isEnabled() ? 1.0f : 0.5f));
    g.strokePath (path, PathStrokeType (1.9f, PathStrokeType::curved, PathStrokeType::rounded));
    if (hasText)
    {
        g.setColour (primary ? p.onAccent : p.ink);
        g.setFont (uiFont (13.0f, primary));
        g.drawText (getButtonText(), r.withTrimmedLeft (36.0f).withTrimmedRight (12.0f), Justification::centredLeft, true);
    }
}

//==============================================================================
void askForText (const String& title, const String& message, const String& initial, std::function<void (const String&)> onOk)
{
    auto* aw = new AlertWindow (title, message, MessageBoxIconType::NoIcon);
    aw->addTextEditor ("text", initial);
    aw->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
    aw->addButton (tr ("Отмена", "Cancel"), 0, KeyPress (KeyPress::escapeKey));
    aw->enterModalState (true, ModalCallbackFunction::create ([aw, onOk] (int r)
    {
        std::unique_ptr<AlertWindow> owner (aw);
        if (r == 1 && onOk)
            onOk (aw->getTextEditorContents ("text").trim());
    }), false);
}

void AppState::newCategory (std::function<void (const String&)> then)
{
    askForText (tr ("Новая категория", "New category"),
                tr ("Как назовём? Потом раздавай её плагинам правым кликом по карточке.",
                    "Name it. Then give it to plugins with a right-click on a card."),
                {}, [this, then] (const String& name)
    {
        if (name.isEmpty())
            return;
        if (! lib.addCategory (name) && ! allCategories().contains (name))
            return;
        if (categoriesChanged) categoriesChanged();
        if (then) then (name);
    });
}

Image AppState::thumbFor (const String& key)
{
    auto it = thumbs.find (key);
    if (it != thumbs.end())
        return it->second;
    const auto f = lib.imageFile (key);
    Image img;
    if (f.existsAsFile())
        img = ImageFileFormat::loadFrom (f);
    thumbs[key] = img;
    return img;
}

int AppState::monthsSince (const Plugin& p) const
{
    if (p.lastUsed.toMilliseconds() == 0)
        return 9999;
    const auto days = (Time::getCurrentTime() - p.lastUsed).inDays();
    return jmax (0, (int) std::floor (days / 30.44));
}

AppState::Usage AppState::usage (const Plugin& p) const
{
    if (p.uses == 0)
        return Usage::Never;
    if (monthsSince (p) >= lib.staleMonths)
        return Usage::Stale;
    const double years = jmax (1.0, (Time::getCurrentTime() - p.firstUsed).inDays() / 365.25);
    const double perYear = p.uses / years;
    return perYear >= 12.0 ? Usage::Often : Usage::Sometimes;
}

bool AppState::canDelete (const Plugin& p) const
{
    if (lib.projectCache.empty())
        return false;
    const auto u = usage (p);
    return u == Usage::Never || u == Usage::Stale;
}

String AppState::lastUsedText (const Plugin& p, bool longYear) const
{
    if (p.lastUsed.toMilliseconds() == 0)
        return {};
    return p.lastUsed.formatted (longYear ? "%d.%m.%Y" : "%d.%m.%y");
}

String AppState::frequencyText (const Plugin& p) const
{
    const auto u = usage (p);
    if (u == Usage::Never)
        return tr ("ни разу", "never");
    const double years = jmax (1.0, (Time::getCurrentTime() - p.firstUsed).inDays() / 365.25);
    const double perYear = p.uses / years;
    const auto rate = perYear >= 10.0 ? String (roundToInt (perYear)) : String (perYear, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
    const auto rateText = tr ("~", "~") + rate + tr (" в год", "/yr");
    const int m = monthsSince (p);
    const auto ago = m < 1 ? tr ("в этом месяце", "this month") : String (m) + tr (" мес. назад", " mo ago");
    switch (u)
    {
        case Usage::Often:     return tr ("часто · ", "often · ") + rateText;
        case Usage::Sometimes: return tr ("иногда · ", "sometimes · ") + rateText;
        case Usage::Stale:     return tr ("давно · ", "stale · ") + ago;
        default: break;
    }
    return {};
}
