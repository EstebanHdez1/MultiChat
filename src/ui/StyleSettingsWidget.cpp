#include "StyleSettingsWidget.h"
#include "ConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QFontDatabase>
#include <QIcon>
#include <QBuffer>

static QString previewLogoDataUri(const QString& resourcePath)
{
    const QPixmap pixmap = QIcon(resourcePath).pixmap(18, 18);
    if (pixmap.isNull()) return {};
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, "PNG");
    return QString("data:image/png;base64,%1").arg(QString::fromLatin1(bytes.toBase64()));
}

StyleSettingsWidget::StyleSettingsWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    loadCurrentStyle();
}

void StyleSettingsWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(16);

    auto* titleLayout = new QHBoxLayout();
    auto* titleIcon = new QLabel(this);
    titleIcon->setPixmap(QIcon(":/icons/style.svg").pixmap(20, 20));
    auto* titleLabel = new QLabel("ESTILO Y PERSONALIZACIÓN VISUAL DEL CHAT", this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #FFFFFF;");
    titleLayout->addWidget(titleIcon);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    mainLayout->addLayout(titleLayout);

    auto* contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(20);

    // Left Column: Options
    auto* optionsWidget = new QWidget(this);
    auto* optLayout = new QVBoxLayout(optionsWidget);
    optLayout->setContentsMargins(0, 0, 0, 0);
    optLayout->setSpacing(12);

    // Presets Row
    auto* presetGroup = new QFrame(this);
    presetGroup->setStyleSheet("background: #1A1D24; border-radius: 8px; padding: 10px; border: 1px solid #2D3748;");
    auto* pLayout = new QVBoxLayout(presetGroup);
    
    auto* lblPresets = new QLabel("PRESETS DE TEMA", presetGroup);
    lblPresets->setStyleSheet("font-size: 11px; font-weight: bold; color: #9CA3AF;");
    pLayout->addWidget(lblPresets);

    auto* btnPresetLayout = new QHBoxLayout();
    auto createPresetBtn = [this, btnPresetLayout](const QString& name, ThemePreset preset) {
        auto* btn = new QPushButton(name, this);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet("QPushButton { background: #2D3748; color: white; border: none; border-radius: 6px; padding: 6px 12px; font-size: 11px; font-weight: 500; } QPushButton:hover { background: #3B82F6; }");
        connect(btn, &QPushButton::clicked, this, [this, preset]() {
            onApplyPreset(preset);
        });
        btnPresetLayout->addWidget(btn);
    };

    createPresetBtn("Dark", ThemePreset::DARK);
    createPresetBtn("Light", ThemePreset::LIGHT);
    createPresetBtn("Streamer", ThemePreset::STREAMER);
    createPresetBtn("Minimal", ThemePreset::MINIMAL);
    createPresetBtn("OBS transparente", ThemePreset::TRANSPARENT);

    pLayout->addLayout(btnPresetLayout);
    optLayout->addWidget(presetGroup);

    // Typography Group
    auto* typoGroup = new QFrame(this);
    typoGroup->setStyleSheet("background: #1A1D24; border-radius: 8px; padding: 10px; border: 1px solid #2D3748;");
    auto* tLayout = new QVBoxLayout(typoGroup);

    auto* lblTypo = new QLabel("TIPOGRAFÍA Y TAMAÑO", typoGroup);
    lblTypo->setStyleSheet("font-size: 11px; font-weight: bold; color: #9CA3AF;");
    tLayout->addWidget(lblTypo);

    auto* fLayout = new QHBoxLayout();
    auto* lblFont = new QLabel("Fuente:", typoGroup);
    lblFont->setStyleSheet("color: #D1D5DB; font-size: 12px;");
    m_cmbFontFamily = new QComboBox(typoGroup);
    m_cmbFontFamily->addItems({ "Segoe UI", "Arial", "Roboto", "Consolas", "Trebuchet MS", "Inter", "Tahoma", "Verdana" });
    m_cmbFontFamily->setStyleSheet("background: #111827; color: white; border: 1px solid #374151; border-radius: 4px; padding: 4px 8px;");
    connect(m_cmbFontFamily, &QComboBox::currentTextChanged, this, &StyleSettingsWidget::onFontFamilyChanged);
    fLayout->addWidget(lblFont);
    fLayout->addWidget(m_cmbFontFamily, 1);
    tLayout->addLayout(fLayout);

    auto* sLayout = new QHBoxLayout();
    auto* lblSize = new QLabel("Tamaño:", typoGroup);
    lblSize->setStyleSheet("color: #D1D5DB; font-size: 12px;");
    m_sliderFontSize = new QSlider(Qt::Horizontal, typoGroup);
    m_sliderFontSize->setRange(10, 28);
    m_sliderFontSize->setValue(14);
    m_spnFontSize = new QSpinBox(typoGroup);
    m_spnFontSize->setRange(10, 28);
    m_spnFontSize->setValue(14);
    m_spnFontSize->setStyleSheet("background: #111827; color: white; border: 1px solid #374151; border-radius: 4px; padding: 2px;");
    
    connect(m_sliderFontSize, &QSlider::valueChanged, m_spnFontSize, &QSpinBox::setValue);
    connect(m_spnFontSize, QOverload<int>::of(&QSpinBox::valueChanged), m_sliderFontSize, &QSlider::setValue);
    connect(m_sliderFontSize, &QSlider::valueChanged, this, &StyleSettingsWidget::onFontSizeChanged);

    sLayout->addWidget(lblSize);
    sLayout->addWidget(m_sliderFontSize, 1);
    sLayout->addWidget(m_spnFontSize);
    tLayout->addLayout(sLayout);

    optLayout->addWidget(typoGroup);

    // Toggles Group
    auto* togglesGroup = new QFrame(this);
    togglesGroup->setStyleSheet("background: #1A1D24; border-radius: 8px; padding: 10px; border: 1px solid #2D3748;");
    auto* togLayout = new QVBoxLayout(togglesGroup);

    auto* lblToggles = new QLabel("ELEMENTOS VISUALES", togglesGroup);
    lblToggles->setStyleSheet("font-size: 11px; font-weight: bold; color: #9CA3AF;");
    togLayout->addWidget(lblToggles);

    auto createToggle = [this, togLayout, togglesGroup](const QString& text) -> QCheckBox* {
        auto* chk = new QCheckBox(text, togglesGroup);
        chk->setChecked(true);
        chk->setCursor(Qt::PointingHandCursor);
        chk->setStyleSheet("QCheckBox { color: #D1D5DB; font-size: 12px; }");
        connect(chk, &QCheckBox::toggled, this, &StyleSettingsWidget::onSettingToggled);
        togLayout->addWidget(chk);
        return chk;
    };

    m_chkTimestamps = createToggle("Mostrar timestamps [12:34:56]");
    m_chkBadges = createToggle("Mostrar insignias de plataforma");
    m_chkUsernames = createToggle("Mostrar nombres de usuario");
    m_chkDonationHighlights = createToggle("Resaltar eventos de donaciones");
    m_chkSubHighlights = createToggle("Resaltar suscripciones y miembros");

    optLayout->addWidget(togglesGroup);
    optLayout->addStretch();

    contentLayout->addWidget(optionsWidget, 1);

    // Right Column: Live Preview
    auto* previewFrame = new QFrame(this);
    previewFrame->setStyleSheet("background: #1A1D24; border-radius: 10px; border: 1px solid #2D3748; padding: 12px;");
    auto* prevLayout = new QVBoxLayout(previewFrame);

    auto* lblPrev = new QLabel("VISTA PREVIA EN TIEMPO REAL", previewFrame);
    lblPrev->setStyleSheet("font-size: 12px; font-weight: bold; color: #9CA3AF;");
    prevLayout->addWidget(lblPrev);

    m_previewBrowser = new QTextBrowser(previewFrame);
    m_previewBrowser->setReadOnly(true);
    m_previewBrowser->setStyleSheet("border: 1px solid #374151; border-radius: 8px; padding: 8px;");
    prevLayout->addWidget(m_previewBrowser, 1);

    contentLayout->addWidget(previewFrame, 1);

    mainLayout->addLayout(contentLayout, 1);
}

void StyleSettingsWidget::loadCurrentStyle()
{
    m_currentStyle = ConfigManager::instance().getStyleConfig();

    m_cmbFontFamily->setCurrentText(m_currentStyle.fontFamily);
    m_sliderFontSize->setValue(m_currentStyle.fontSize);
    m_spnFontSize->setValue(m_currentStyle.fontSize);

    m_chkTimestamps->setChecked(m_currentStyle.showTimestamps);
    m_chkBadges->setChecked(m_currentStyle.showPlatformBadges);
    m_chkUsernames->setChecked(m_currentStyle.showUsernames);
    m_chkDonationHighlights->setChecked(m_currentStyle.showDonationHighlights);
    m_chkSubHighlights->setChecked(m_currentStyle.showSubscriptionHighlights);

    updatePreview();
}

void StyleSettingsWidget::onApplyPreset(ThemePreset preset)
{
    ConfigManager::instance().applyPreset(preset);
    loadCurrentStyle();
}

void StyleSettingsWidget::onFontFamilyChanged(const QString& font)
{
    m_currentStyle.fontFamily = font;
    ConfigManager::instance().setStyleConfig(m_currentStyle);
    updatePreview();
}

void StyleSettingsWidget::onFontSizeChanged(int size)
{
    m_currentStyle.fontSize = size;
    ConfigManager::instance().setStyleConfig(m_currentStyle);
    updatePreview();
}

void StyleSettingsWidget::onSettingToggled()
{
    m_currentStyle.showTimestamps = m_chkTimestamps->isChecked();
    m_currentStyle.showPlatformBadges = m_chkBadges->isChecked();
    m_currentStyle.showUsernames = m_chkUsernames->isChecked();
    m_currentStyle.showDonationHighlights = m_chkDonationHighlights->isChecked();
    m_currentStyle.showSubscriptionHighlights = m_chkSubHighlights->isChecked();

    ConfigManager::instance().setStyleConfig(m_currentStyle);
    updatePreview();
}

void StyleSettingsWidget::updatePreview()
{
    QString bg = m_currentStyle.backgroundColor;
    if (bg == "transparent") bg = "#0B0C10";

    m_previewBrowser->setStyleSheet(QString(
        "QTextBrowser {"
        "  background-color: %1;"
        "  border: 1px solid #374151;"
        "  border-radius: %2px;"
        "  color: %3;"
        "  font-family: '%4';"
        "  font-size: %5px;"
        "  padding: 10px;"
        "}"
    ).arg(bg).arg(m_currentStyle.borderRadius).arg(m_currentStyle.textColor, m_currentStyle.fontFamily).arg(m_currentStyle.fontSize));

    QString html;

    auto formatLine = [this](const QString& platform, const QString& iconPath, const QString& user, const QString& userColor, const QString& text, bool isDonation = false, const QString& donAmount = "") -> QString {
        QString lineBg = "rgba(255, 255, 255, 0.03)";
        if (isDonation && m_currentStyle.showDonationHighlights)
        {
            lineBg = "rgba(234, 179, 8, 0.15); border-left: 3px solid #EAB308";
        }
        QString line = QString("<div style=\"margin-bottom: 6px; padding: 4px 8px; border-radius: 6px; background-color: %1;\">")
            .arg(lineBg);

        if (m_currentStyle.showPlatformBadges)
        {
            line += QString("<span title=\"%1\" style=\"background: transparent; margin-right: 6px; vertical-align: middle;\"><img src=\"%2\" width=\"16\" height=\"16\" /></span>")
                .arg(platform.toHtmlEscaped(), previewLogoDataUri(iconPath));
        }

        if (m_currentStyle.showTimestamps)
        {
            line += QString("<span style=\"color: %1; font-size: 11px; margin-right: 5px;\">[14:30:00]</span>")
                .arg(m_currentStyle.timestampColor);
        }

        if (m_currentStyle.showUsernames)
        {
            line += QString("<span style=\"color: %1; font-weight: bold; margin-right: 5px;\">%2:</span>")
                .arg(userColor, user);
        }

        if (isDonation && !donAmount.isEmpty())
        {
            line += QString("<span style=\"background-color: #F59E0B; color: #000000; font-size: 10px; font-weight: bold; padding: 1px 5px; border-radius: 3px; margin-right: 5px;\">%1</span>")
                .arg(donAmount);
        }

        line += QString("<span style=\"color: %1;\">%2</span>").arg(m_currentStyle.textColor, text);
        line += "</div>";
        return line;
    };

    html += formatLine("Twitch", ":/icons/twitch.svg", "TwitchFan", "#9146FF", "¡Hola a todos desde Twitch!");
    html += formatLine("YouTube", ":/icons/youtube.svg", "SuperUser", "#EF4444", "¡Gran directo amigo! Saludos", true, "$20.00");
    html += formatLine("Kick", ":/icons/kick.svg", "KickGamer", "#53FC18", "Buenas tardes comunidad de Kick");
    html += formatLine("TikTok", ":/icons/tiktok.svg", "TikToker", m_currentStyle.usernameColor, "Increíble stream!");

    m_previewBrowser->setHtml(html);
}
