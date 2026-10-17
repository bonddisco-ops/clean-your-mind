#include "Ui.h"
#include "Logo.h"

using namespace juce;

CropEditor::CropEditor (AppState& a, const String& k, Image original, bool isNew) : app (a), key (k), img (original), fresh (isNew)
{
    const auto it = app.lib.imageStates.find (key);
    if (! isNew && it != app.lib.imageStates.end())
        st = it->second;

    zoom.setSliderStyle (Slider::LinearHorizontal);
    zoom.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    zoom.setRange (0.25, 4.0, 0.01);
    zoom.setSkewFactorFromMidPoint (1.0);
    zoom.setValue (st.zoom, dontSendNotification);
    zoom.onValueChange = [this] { st.zoom = (float) zoom.getValue(); clampCentre(); repaint(); };
    addAndMakeVisible (zoom);

    fill.setButtonText (tr ("Заполнить", "Fill"));
    fit.setButtonText (tr ("Вписать", "Fit"));
    stretch.setButtonText (tr ("Растянуть", "Stretch"));
    int m = 0;
    for (auto* b : { &fill, &fit, &stretch })
    {
        const int mode = m++;
        b->getProperties().set ("pill", false);
        b->onClick = [this, mode] { setMode (mode); };
        addAndMakeVisible (*b);
    }
    cancel.setButtonText (tr ("Отмена", "Cancel"));
    save.setButtonText (tr ("Сохранить", "Save"));
    cancel.getProperties().set ("pill", false);
    save.getProperties().set ("pill", false);
    save.getProperties().set ("primary", true);
    cancel.onClick = [this] { if (onClose) onClose(); };
    save.onClick = [this]
    {
        const auto thumb = render (504, roundToInt (504.0f / PluginCard::imageAspect));
        auto f = app.lib.imageFile (key);
        f.deleteFile();
        if (auto out = f.createOutputStream())
            PNGImageFormat().writeImageToStream (thumb, *out);
        const auto orig = app.lib.originalImageFile (key);
        if (fresh || ! orig.existsAsFile())
        {
            orig.deleteFile();
            if (auto out = orig.createOutputStream())
                PNGImageFormat().writeImageToStream (img, *out);
        }
        app.lib.imageStates[key] = st;
        app.lib.saveUserData();
        app.thumbs.erase (key);
        if (app.refreshAll) app.refreshAll();
        if (onClose) onClose();
    };
    addAndMakeVisible (cancel);
    addAndMakeVisible (save);
    setMode (st.mode);
    setWantsKeyboardFocus (true);
}

void CropEditor::setMode (int m)
{
    st.mode = m;
    int k = 0;
    for (auto* b : { &fill, &fit, &stretch })
        b->setToggleState (k++ == m, dontSendNotification);
    clampCentre();
    repaint();
}

Rectangle<float> CropEditor::imageRectInFrame (Rectangle<float> f) const
{
    if (! img.isValid())
        return f;
    const float iw = (float) img.getWidth(), ih = (float) img.getHeight();
    float w, h;
    if (st.mode == 2)
    {
        w = f.getWidth() * st.zoom;
        h = f.getHeight() * st.zoom;
    }
    else
    {
        const float base = st.mode == 0 ? jmax (f.getWidth() / iw, f.getHeight() / ih) : jmin (f.getWidth() / iw, f.getHeight() / ih);
        w = iw * base * st.zoom;
        h = ih * base * st.zoom;
    }
    return { f.getCentreX() - st.cx * w, f.getCentreY() - st.cy * h, w, h };
}

void CropEditor::clampCentre()
{
    const auto ref = Rectangle<float> (0, 0, 1000.0f, 1000.0f / PluginCard::imageAspect);
    auto r = imageRectInFrame (ref);
    auto clampAxis = [] (float& c, float size, float frameSize)
    {
        if (size <= frameSize) c = 0.5f;
        else c = jlimit (frameSize * 0.5f / size, 1.0f - frameSize * 0.5f / size, c);
    };
    clampAxis (st.cx, r.getWidth(), ref.getWidth());
    clampAxis (st.cy, r.getHeight(), ref.getHeight());
}

Image CropEditor::render (int w, int h) const
{
    Image out (Image::ARGB, w, h, true);
    Graphics g (out);
    g.fillAll (Colour (0xff0c0c0e));
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    const auto r = imageRectInFrame (Rectangle<float> ((float) w, (float) h));
    g.drawImage (img, r, RectanglePlacement::stretchToFit);
    return out;
}

void CropEditor::resized()
{
    card = Rectangle<int> (jmin (1000, getWidth() - 60), jmin (620, getHeight() - 60)).withCentre (getLocalBounds().getCentre());
    auto body = card.reduced (22).withTrimmedTop (70);
    auto right = body.removeFromRight (270);
    body.removeFromRight (22);
    auto controls = body.removeFromBottom (40);
    body.removeFromBottom (14);
    stage = body;
    const float fw = jmin ((float) stage.getWidth() - 80.0f, ((float) stage.getHeight() - 60.0f) * PluginCard::imageAspect);
    frame = Rectangle<float> (fw, fw / PluginCard::imageAspect).withCentre (stage.toFloat().getCentre());

    auto modes = controls.removeFromRight (300);
    const int mw = (modes.getWidth() - 8) / 3;
    fill.setBounds (modes.removeFromLeft (mw).reduced (0, 3)); modes.removeFromLeft (4);
    fit.setBounds (modes.removeFromLeft (mw).reduced (0, 3)); modes.removeFromLeft (4);
    stretch.setBounds (modes.reduced (0, 3));
    controls.removeFromLeft (80);
    controls.removeFromRight (60);
    zoom.setBounds (controls);

    preview = Rectangle<int> (right.getX(), right.getY() + 26, right.getWidth(), roundToInt ((float) right.getWidth() / PluginCard::imageAspect));
    auto bottom = right.removeFromBottom (40);
    cancel.setBounds (bottom.removeFromLeft ((bottom.getWidth() - 8) / 2));
    bottom.removeFromLeft (8);
    save.setBounds (bottom);
}

void CropEditor::paint (Graphics& g)
{
    const auto& c = palOf (*this);
    g.fillAll (Colours::black.withAlpha (0.72f));
    const auto cf = card.toFloat();
    g.setColour (c.bg);
    g.fillRoundedRectangle (cf, 16.0f);
    g.setColour (c.stroke.withMultipliedAlpha (1.5f));
    g.drawRoundedRectangle (cf.reduced (0.5f), 16.0f, 1.0f);

    // header
    Logo::draw (g, Rectangle<float> ((float) card.getX() + 22.0f, (float) card.getY() + 18.0f, 30.0f, 32.0f), FillType (themeOf (*this).logo));
    const auto* p = app.lib.find (key);
    g.setColour (c.ink);
    g.setFont (uiFont (17.0f, true, 0.05f));
    g.drawText (tr ("КАРТИНКА ДЛЯ: ", "PICTURE FOR: ") + (p ? p->name.toUpperCase() : String()),
                card.getX() + 66, card.getY() + 16, card.getWidth() - 90, 22, Justification::centredLeft, true);
    g.setColour (c.mute);
    g.setFont (uiFont (11.5f));
    g.drawText (tr ("Тащи картинку, чтобы двигать. Колесо мыши меняет масштаб. Ctrl+V вставит скрин из буфера.",
                    "Drag to move. Mouse wheel zooms. Ctrl+V pastes a screenshot from the clipboard."),
                card.getX() + 66, card.getY() + 40, card.getWidth() - 90, 18, Justification::centredLeft, true);
    g.setColour (c.stroke);
    g.drawHorizontalLine (card.getY() + 70, (float) card.getX(), (float) card.getRight());

    // stage
    {
        Graphics::ScopedSaveState ss (g);
        const auto sf = stage.toFloat();
        Path clip;
        clip.addRoundedRectangle (sf, 10.0f);
        g.reduceClipRegion (clip);
        g.setColour (Colour (0xff0c0c0e));
        g.fillRect (sf);
        g.setImageResamplingQuality (Graphics::mediumResamplingQuality);
        g.drawImage (img, imageRectInFrame (frame), RectanglePlacement::stretchToFit);
        // dim outside the frame
        Path outside;
        outside.addRectangle (sf);
        outside.addRoundedRectangle (frame, 6.0f);
        outside.setUsingNonZeroWinding (false);
        g.setColour (Colours::black.withAlpha (0.62f));
        g.fillPath (outside);
        // thirds
        g.setColour (c.accent.withAlpha (0.3f));
        for (int i = 1; i < 3; ++i)
        {
            g.drawVerticalLine (roundToInt (frame.getX() + frame.getWidth() * (float) i / 3.0f), frame.getY(), frame.getBottom());
            g.drawHorizontalLine (roundToInt (frame.getY() + frame.getHeight() * (float) i / 3.0f), frame.getX(), frame.getRight());
        }
        g.setColour (c.accent);
        g.drawRoundedRectangle (frame, 6.0f, 2.0f);
        for (auto pt : { frame.getTopLeft(), frame.getTopRight(), frame.getBottomLeft(), frame.getBottomRight() })
            g.fillRoundedRectangle (Rectangle<float> (10.0f, 10.0f).withCentre (pt), 2.0f);
    }

    // zoom label and value
    g.setColour (c.mute);
    g.setFont (uiFont (12.0f));
    g.drawText (tr ("Масштаб", "Zoom"), zoom.getX() - 80, zoom.getY(), 76, zoom.getHeight(), Justification::centredLeft);
    g.setColour (c.ink);
    g.drawText (String (roundToInt (st.zoom * 100.0f)) + "%", zoom.getRight() + 8, zoom.getY(), 52, zoom.getHeight(), Justification::centredLeft);

    // preview card
    g.setColour (c.mute);
    g.setFont (uiFont (10.0f, true, 0.12f));
    g.drawText (tr ("КАК БУДЕТ В ГАЛЕРЕЕ", "HOW IT LOOKS IN THE GALLERY"), preview.getX(), preview.getY() - 24, preview.getWidth(), 18, Justification::centredLeft);
    const auto pv = preview.toFloat();
    const auto cardR = pv.withHeight (pv.getHeight() + 56.0f);
    g.setColour (c.panel);
    g.fillRoundedRectangle (cardR, 12.0f);
    {
        Graphics::ScopedSaveState ss (g);
        Path clip;
        clip.addRoundedRectangle (pv.getX(), pv.getY(), pv.getWidth(), pv.getHeight(), 12.0f, 12.0f, true, true, false, false);
        g.reduceClipRegion (clip);
        g.drawImageAt (render (preview.getWidth(), preview.getHeight()), preview.getX(), preview.getY());
    }
    g.setColour (c.stroke);
    g.drawRoundedRectangle (cardR.reduced (0.5f), 12.0f, 1.0f);
    if (p != nullptr)
    {
        g.setColour (c.ink);
        g.setFont (uiFont (14.0f, true));
        g.drawText (p->name, preview.getX() + 12, preview.getBottom() + 8, preview.getWidth() - 24, 20, Justification::centredLeft, true);
        g.setColour (c.mute);
        g.setFont (uiFont (11.0f));
        g.drawText (p->vendor, preview.getX() + 12, preview.getBottom() + 28, preview.getWidth() - 24, 18, Justification::centredLeft, true);
    }
    g.setColour (c.mute);
    g.setFont (uiFont (11.5f));
    g.drawFittedText (tr ("Рамка у всех одна, поэтому карточки выходят одинаковыми. «Растянуть» убирает поля, но может немного сплющить картинку.",
                          "Every card uses the same frame, so the grid stays even. Stretch removes borders but may squash the picture."),
                      Rectangle<int> (preview.getX(), preview.getBottom() + 72, preview.getWidth(), 70), Justification::topLeft, 5);
}

void CropEditor::mouseDown (const MouseEvent& e)
{
    draggingImage = stage.contains (e.getPosition());
    dragStartCentre = { st.cx, st.cy };
    dragStartMouse = e.position;
    if (! card.contains (e.getPosition()) && onClose)
        onClose();
}

void CropEditor::mouseDrag (const MouseEvent& e)
{
    if (! draggingImage)
        return;
    const auto r = imageRectInFrame (frame);
    const auto d = e.position - dragStartMouse;
    st.cx = dragStartCentre.x - d.x / jmax (1.0f, r.getWidth());
    st.cy = dragStartCentre.y - d.y / jmax (1.0f, r.getHeight());
    clampCentre();
    repaint();
}

void CropEditor::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    if (! stage.contains (e.getPosition()))
        return;
    st.zoom = jlimit (0.25f, 4.0f, st.zoom * std::pow (1.12f, w.deltaY * 8.0f));
    zoom.setValue (st.zoom, dontSendNotification);
    clampCentre();
    repaint();
}
