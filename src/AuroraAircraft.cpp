/*
 * AuroraAircraft — plugin Stellarium pokazujący samoloty na żywo.
 * Copyright (C) 2026 astronow.pl / Kamil Zaras
 *
 * Licensed under GNU GPL v2 or later (Stellarium-compatible).
 */

#include "AuroraAircraft.hpp"

#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelModuleMgr.hpp"
#include "StelPainter.hpp"
#include "StelProjector.hpp"

#include <QDebug>
#include <QFont>
#include <QGuiApplication>


//
// StelPluginInterface — to wywołuje StelModuleMgr po załadowaniu .dylib.
//

StelModule* AuroraAircraftStelPluginInterface::getStelModule() const
{
	return new AuroraAircraft();
}

StelPluginInfo AuroraAircraftStelPluginInterface::getPluginInfo() const
{
	StelPluginInfo info;
	info.id              = "AuroraAircraft";
	info.displayedName   = "Aurora Aircraft (live ADS-B)";
	info.authors         = "astronow.pl / Kamil Zaras";
	info.contact         = "https://astronow.pl";
	info.description     = "Pokazuje samoloty na żywo na sferze niebieskiej "
	                       "na podstawie danych ADS-B (adsb.fi). "
	                       "Side-loadowy plugin dla patronów astronow.pl.";
	info.version         = "0.0.1";
	info.license         = "GPL v2 or later";
	return info;
}


//
// AuroraAircraft — klasa StelModule, ładowana do Stellarium.
//

AuroraAircraft::AuroraAircraft() : fontSize(20)
{
	setObjectName("AuroraAircraft");
}

AuroraAircraft::~AuroraAircraft()
{
}

void AuroraAircraft::init()
{
	qDebug() << "[AuroraAircraft] init() called — plugin loaded";
}

double AuroraAircraft::getCallOrder(StelModuleActionName actionName) const
{
	// Rysuj po standardowym Nebula manager — żeby napis był na wierzchu.
	if (actionName == StelModule::ActionDraw)
		return StelApp::getInstance().getModuleMgr().getModule("NebulaMgr")->getCallOrder(actionName) + 10.0;
	return 0;
}

void AuroraAircraft::draw(StelCore* core)
{
	// MVP step 0: tylko sanity-check, że plugin się ładuje i renderer działa.
	StelPainter painter(core->getProjection2d());
	painter.setColor(0.4f, 0.85f, 1.0f, 1.0f); // niebieski jak nasze ikony jet
	QFont font = QGuiApplication::font();
	font.setPixelSize(fontSize);
	painter.setFont(font);
	painter.drawText(40, 80, "AuroraAircraft v0.0.1 — plugin loaded");
}
