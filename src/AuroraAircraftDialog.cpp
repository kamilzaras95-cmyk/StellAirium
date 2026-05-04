/*
 * AuroraAircraft — plugin Stellarium pokazujący samoloty na żywo.
 * Copyright (C) 2026 astronow.pl / Kamil Zaras
 * Licensed under GNU GPL v2 or later.
 */

#include "AuroraAircraftDialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

// Indeks 0/1 = predefiniowane źródła, 2 = własny URL.
static const struct { const char* label; const char* urlTemplate; } kSources[] = {
    { "adsb.fi",        "https://opendata.adsb.fi/api/v2/lat/%1/lon/%2/dist/%3" },
    { "airplanes.live", "https://api.airplanes.live/v2/point/%1/%2/%3"          },
    { "Własny URL...",  "custom"                                                 },
};
static constexpr int kSourceCount = static_cast<int>(sizeof(kSources) / sizeof(kSources[0]));

// Slider value × 25 = nm. Range 2..20 → 50..500 nm.
static constexpr int kSliderFactor = 25;

AuroraAircraftDialog::AuroraAircraftDialog(QWidget* parent)
    : QDialog(parent, Qt::Window)
{
    setWindowTitle("StellAirium — konfiguracja");
    setMinimumWidth(380);

    // Stellarium ustawia swój QSS na QApplication — nadpisujemy go dla tego okna,
    // żeby combo boxy i przyciski były czytelne na ciemnym tle.
    setStyleSheet(R"(
        QDialog, QWidget { background-color: #2b2b2b; color: #cccccc; }
        QGroupBox {
            border: 1px solid rgba(255,255,255,0.18);
            border-radius: 4px;
            margin-top: 8px;
            padding: 4px 4px 4px 4px;
            color: #cccccc;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 8px;
            padding: 0 3px;
        }
        QLabel { color: #cccccc; background: transparent; }
        QComboBox {
            background-color: #3c3f41;
            color: #cccccc;
            border: 1px solid #555;
            border-radius: 3px;
            padding: 3px 6px;
            min-width: 8em;
        }
        QComboBox::drop-down { border: none; width: 18px; }
        QComboBox::down-arrow { image: none; border-left: 4px solid transparent; border-right: 4px solid transparent; border-top: 5px solid #aaa; margin-right: 4px; }
        QComboBox QAbstractItemView {
            background-color: #3c3f41;
            color: #cccccc;
            selection-background-color: #2d5a8e;
            selection-color: #ffffff;
            border: 1px solid #555;
        }
        QSlider::groove:horizontal {
            height: 4px; background: #555; border-radius: 2px; margin: 0;
        }
        QSlider::handle:horizontal {
            background: #aaaaaa; width: 14px; height: 14px;
            margin: -5px 0; border-radius: 7px;
        }
        QSlider::handle:horizontal:hover { background: #dddddd; }
        QPushButton {
            background-color: #3c3f41;
            color: #cccccc;
            border: 1px solid #555;
            border-radius: 3px;
            padding: 5px 14px;
            min-width: 60px;
        }
        QPushButton:hover { background-color: #4c5052; }
        QPushButton:pressed { background-color: #2d5a8e; }
        QLineEdit {
            background-color: #3c3f41;
            color: #cccccc;
            border: 1px solid #555;
            border-radius: 3px;
            padding: 3px 6px;
        }
        QFrame[frameShape="4"] { color: rgba(255,255,255,0.15); }
    )");

    auto* main = new QVBoxLayout(this);
    main->setSpacing(10);
    main->setContentsMargins(14, 14, 14, 14);

    // === Status ===
    statusLabel = new QLabel("Czekam na pierwsze dane...", this);
    statusLabel->setStyleSheet("color: #55AAFF;");
    main->addWidget(statusLabel);

    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    main->addWidget(sep);

    // === Zasięg ===
    auto* rangeBox = new QGroupBox("Zasięg zapytania", this);
    auto* rangeL   = new QVBoxLayout(rangeBox);

    auto* sliderRow = new QHBoxLayout();
    distSlider = new QSlider(Qt::Horizontal, this);
    distSlider->setMinimum(2);    // 50 nm
    distSlider->setMaximum(20);   // 500 nm
    distSlider->setValue(10);     // 250 nm
    distSlider->setTickPosition(QSlider::TicksBelow);
    distSlider->setTickInterval(2);

    distValueLabel = new QLabel("250 nm", this);
    distValueLabel->setMinimumWidth(56);
    distValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    sliderRow->addWidget(distSlider);
    sliderRow->addWidget(distValueLabel);
    rangeL->addLayout(sliderRow);

    auto* hintRow = new QHBoxLayout();
    auto* hintMin = new QLabel("50 nm", this);
    auto* hintMax = new QLabel("500 nm", this);
    hintMin->setStyleSheet("color: gray; font-size: 10px;");
    hintMax->setStyleSheet("color: gray; font-size: 10px;");
    hintRow->addWidget(hintMin);
    hintRow->addStretch();
    hintRow->addWidget(hintMax);
    rangeL->addLayout(hintRow);
    main->addWidget(rangeBox);

    // === Źródło danych ===
    auto* srcBox = new QGroupBox("Źródło danych ADS-B", this);
    auto* srcL   = new QFormLayout(srcBox);

    sourceCombo = new QComboBox(this);
    for (int i = 0; i < kSourceCount; ++i)
        sourceCombo->addItem(kSources[i].label, QString(kSources[i].urlTemplate));
    srcL->addRow("Serwer:", sourceCombo);

    customUrlEdit = new QLineEdit(this);
    customUrlEdit->setPlaceholderText("https://host/api/lat/%1/lon/%2/dist/%3");
    customUrlEdit->setVisible(false);
    srcL->addRow("URL:", customUrlEdit);
    main->addWidget(srcBox);

    // === Interwał ===
    auto* intBox = new QGroupBox("Interwał odświeżania", this);
    auto* intL   = new QFormLayout(intBox);
    intervalCombo = new QComboBox(this);
    intervalCombo->addItem("10 s", 10);
    intervalCombo->addItem("15 s (domyślnie)", 15);
    intervalCombo->addItem("30 s", 30);
    intervalCombo->addItem("60 s", 60);
    intervalCombo->setCurrentIndex(1);
    intL->addRow("Co:", intervalCombo);
    main->addWidget(intBox);

    main->addStretch();

    // === Przyciski ===
    auto* btnBox   = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto* fetchBtn = new QPushButton("Fetch teraz", this);
    btnBox->addButton(fetchBtn, QDialogButtonBox::ActionRole);
    main->addWidget(btnBox);

    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(distSlider,   &QSlider::valueChanged,
            this, &AuroraAircraftDialog::onDistSliderChanged);
    connect(sourceCombo,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AuroraAircraftDialog::onSourceChanged);
    connect(customUrlEdit, &QLineEdit::textChanged,
            this, &AuroraAircraftDialog::onCustomUrlChanged);
    connect(intervalCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AuroraAircraftDialog::onIntervalChanged);
    connect(fetchBtn, &QPushButton::clicked,
            this, &AuroraAircraftDialog::fetchNowRequested);
}

void AuroraAircraftDialog::setStatus(int total, int aboveHorizon)
{
    statusLabel->setText(QString("✈ %1 samolotów w promieniu, %2 ponad horyzontem")
                         .arg(total).arg(aboveHorizon));
}

void AuroraAircraftDialog::loadValues(int distNm, const QString& sourceUrlTemplate, int intervalSec)
{
    QSignalBlocker b1(distSlider), b2(sourceCombo), b3(customUrlEdit), b4(intervalCombo);

    distSlider->setValue(distNm / kSliderFactor);
    distValueLabel->setText(QString("%1 nm").arg(distNm));

    bool foundSource = false;
    for (int i = 0; i < kSourceCount - 1; ++i) {
        if (sourceUrlTemplate == kSources[i].urlTemplate) {
            sourceCombo->setCurrentIndex(i);
            customUrlEdit->setVisible(false);
            foundSource = true;
            break;
        }
    }
    if (!foundSource) {
        sourceCombo->setCurrentIndex(kSourceCount - 1);
        customUrlEdit->setText(sourceUrlTemplate);
        customUrlEdit->setVisible(true);
    }

    const int kIntervals[] = {10, 15, 30, 60};
    for (int i = 0; i < intervalCombo->count(); ++i) {
        if (kIntervals[i] == intervalSec) {
            intervalCombo->setCurrentIndex(i);
            break;
        }
    }
}

void AuroraAircraftDialog::onDistSliderChanged(int value)
{
    const int nm = value * kSliderFactor;
    distValueLabel->setText(QString("%1 nm").arg(nm));
    emit distNmChanged(nm);
}

void AuroraAircraftDialog::onSourceChanged(int index)
{
    const bool isCustom = (index == kSourceCount - 1);
    customUrlEdit->setVisible(isCustom);
    adjustSize();
    if (!isCustom)
        emit sourceUrlChanged(QString(kSources[index].urlTemplate));
}

void AuroraAircraftDialog::onCustomUrlChanged(const QString& text)
{
    if (sourceCombo->currentIndex() == kSourceCount - 1 && !text.isEmpty())
        emit sourceUrlChanged(text);
}

void AuroraAircraftDialog::onIntervalChanged(int index)
{
    emit fetchIntervalChanged(intervalCombo->itemData(index).toInt());
}
