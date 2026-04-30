/*
 * AuroraAircraft — plugin Stellarium pokazujący samoloty na żywo.
 * Copyright (C) 2026 astronow.pl / Kamil Zaras
 * Licensed under GNU GPL v2 or later.
 */

#ifndef AIRCRAFTOBJECT_HPP
#define AIRCRAFTOBJECT_HPP

#include "AuroraAircraft.hpp" // dla AircraftSnapshot
#include "StelObject.hpp"

class StelCore;

//! Pojedynczy samolot jako StelObject — żeby Stellarium mogło go selectionować
//! po kliknięciu i pokazać info-panel z hex/altitude/speed/distance.
class AircraftObject : public StelObject
{
public:
	AircraftObject() = default;

	// === pole publiczne, modyfikowane bezpośrednio z update() w AuroraAircraft ===
	AircraftSnapshot snap;

	// === StelObject — pure virtuals do nadpisania ===
	QString getType()         const override { return "AuroraAircraft"; }
	QString getObjectType()   const override { return "Aircraft"; }
	QString getObjectTypeI18n() const override;
	QString getID()           const override { return snap.icao24; }
	QString getEnglishName()  const override;
	QString getNameI18n()     const override { return getEnglishName(); }

	//! AltAz → J2000 unit vector przez Stellarium core (kanoniczna konwersja).
	Vec3d getJ2000EquatorialPos(const StelCore* core) const override;

	//! Bogaty HTML w lewym panelu Stellarium po kliknięciu samolotu.
	//! Pola: callsign, hex, ICAO type, altitude (m + FL), speed (kt + km/h),
	//! kierunek lotu, vertical rate, dystans poziomy do obserwatora.
	QString getInfoString(const StelCore* core, const InfoStringGroup& flags) const override;

	//! Priority do click-resolutionu w `StelObjectMgr::cleverFind`.
	//! Default to magnitude (clamped do 15) — większy niż limitMag (~5) → odrzucany.
	//! Zwracamy -10 (jak Satellites) żeby samoloty wygrywały z gwiazdami przy kliknięciu.
	float getSelectPriority(const StelCore* core) const override { Q_UNUSED(core) return -10.0f; }
};

typedef QSharedPointer<AircraftObject> AircraftObjectP;

#endif // AIRCRAFTOBJECT_HPP
