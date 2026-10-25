#pragma once

#include <QWidget>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include "AppStyleConfig.h"

class StyleSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StyleSettingsWidget(QWidget* parent = nullptr);
    ~StyleSettingsWidget() override = default;

private slots:
    void onFontFamilyChanged(const QString& font);
    void onFontSizeChanged(int size);
    void onSettingToggled();
    void onApplyPreset(ThemePreset preset);

private:
    void setupUi();
    void loadCurrentStyle();
    void updatePreview();

    QComboBox* m_cmbFontFamily;
    QSlider* m_sliderFontSize;
    QSpinBox* m_spnFontSize;

    QCheckBox* m_chkTimestamps;
    QCheckBox* m_chkBadges;
    QCheckBox* m_chkAvatars;
    QCheckBox* m_chkUsernames;
    QCheckBox* m_chkDonationHighlights;
    QCheckBox* m_chkSubHighlights;

    QTextBrowser* m_previewBrowser;
    AppStyleConfig m_currentStyle;
};
