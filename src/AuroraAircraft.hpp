/*
 * AuroraAircraft — plugin Stellarium pokazujący samoloty na żywo.
 * Copyright (C) 2026 astronow.pl / Kamil Zaras
 *
 * Licensed under GNU GPL v2 or later (Stellarium-compatible).
 */

#ifndef AURORAAIRCRAFT_HPP
#define AURORAAIRCRAFT_HPP

#include "StelModule.hpp"
#include <QObject>

class StelCore;

//! Główna klasa pluginu — od razu rysuje napis na ekranie, żeby zweryfikować
//! że plugin się ładuje. W kolejnych iteracjach: fetcher ADS-B, transform AltAz,
//! render ikon samolotów, click-to-detail.
class AuroraAircraft : public StelModule
{
	Q_OBJECT
public:
	AuroraAircraft();
	~AuroraAircraft() override;

	void init() override;
	void draw(StelCore* core) override;
	double getCallOrder(StelModuleActionName actionName) const override;

private:
	int fontSize;
};


#include "StelPluginInterface.hpp"

//! Qt boilerplate — Stellarium używa QPluginLoader żeby załadować bibliotekę,
//! a ten interfejs pozwala mu wciąć getStelModule()/getPluginInfo().
class AuroraAircraftStelPluginInterface : public QObject, public StelPluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID StelPluginInterface_iid)
	Q_INTERFACES(StelPluginInterface)
public:
	StelModule* getStelModule() const override;
	StelPluginInfo getPluginInfo() const override;
	QObjectList getExtensionList() const override { return QObjectList(); }
};

#endif // AURORAAIRCRAFT_HPP
