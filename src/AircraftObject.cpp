/*
 * AuroraAircraft — plugin Stellarium pokazujący samoloty na żywo.
 * Copyright (C) 2026 astronow.pl / Kamil Zaras
 * Licensed under GNU GPL v2 or later.
 */

#include "AircraftObject.hpp"

#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelLocation.hpp"
#include "StelTranslator.hpp"
#include "StelUtils.hpp"

#include <QDebug>
#include <QIODevice>
#include <QString>
#include <QTextStream>
#include <cmath>

QString AircraftObject::getObjectTypeI18n() const
{
	return q_("Aircraft");
}

QString AircraftObject::getEnglishName() const
{
	if (!snap.callsign.isEmpty()) return snap.callsign;
	return snap.icao24.toUpper();
}

Vec3d AircraftObject::getJ2000EquatorialPos(const StelCore* core) const
{
	const StelCore* effectiveCore = core ? core : StelApp::getInstance().getCore();
	if (!effectiveCore)
		return Vec3d(0.0, 0.0, 1.0);

	// Stellarium FrameAltAz: +x=south, +y=east, +z=zenith.
	// lng = (180° - az_geodesic) mod 360°. Patrz AuroraAircraft::draw().
	Vec3d altAzVec;
	const double lng = (180.0 - snap.azDeg) * (M_PI / 180.0);
	const double lat = snap.altDeg * (M_PI / 180.0);
	StelUtils::spheToRect(lng, lat, altAzVec);
	return effectiveCore->altAzToJ2000(altAzVec, StelCore::RefractionOff);
}

QString AircraftObject::getInfoString(const StelCore* core, const InfoStringGroup& flags) const
{
	QString str;
	QTextStream s(&str, QIODevice::WriteOnly);

	// === Nagłówek (Name) ===
	if (flags & Name)
	{
		s << "<h2>";
		const QString cs = snap.callsign.trimmed();
		if (!cs.isEmpty())
			s << cs.toHtmlEscaped();
		else
			s << snap.icao24.toUpper().toHtmlEscaped();
		if (!snap.aircraftType.isEmpty())
			s << "  <small>[" << snap.aircraftType.toHtmlEscaped() << "]</small>";
		s << "</h2>";
	}

	// === Object type ===
	if (flags & ObjectType)
	{
		s << QString(q_("Type: <b>%1</b>")).arg(q_("Aircraft")) << "<br/>";
	}

	// === Pozycja AltAz ===
	if (flags & AltAzi)
	{
		s << QString(q_("Altitude: %1°  Azimuth: %2°"))
		     .arg(QString::number(snap.altDeg, 'f', 2))
		     .arg(QString::number(snap.azDeg, 'f', 2))
		  << "<br/>";
	}

	// === Custom (Extra) — to nasze szczegóły lotu ===
	if (flags & Extra)
	{
		// Hex ICAO24 + (jeśli mamy) registracja z `r` (póki co nie parsujemy `r`,
		// ale plan na wersję 0.0.7 — podobnie jak w naszym webie).
		s << QString(q_("ICAO24 hex: <b>%1</b>")).arg(snap.icao24.toUpper().toHtmlEscaped())
		  << "<br/>";

		// Wysokość — w metrach + Flight Level.
		const int altM_int = static_cast<int>(snap.currentAltM);
		const int fl       = static_cast<int>(snap.currentAltM / 30.48 / 100.0) * 100 / 100;
		s << QString(q_("Altitude (baro): <b>%1 m</b>  (FL%2)")).arg(altM_int).arg(fl)
		  << "<br/>";

		// Prędkość — m/s, knots, km/h.
		const double kt   = snap.speedMs * 1.94384;
		const double kmh  = snap.speedMs * 3.6;
		s << QString(q_("Ground speed: <b>%1 kt</b>  (%2 km/h)"))
		     .arg(QString::number(kt,  'f', 0))
		     .arg(QString::number(kmh, 'f', 0))
		  << "<br/>";

		// Kierunek lotu (true track).
		s << QString(q_("Track: <b>%1°</b> (true)"))
		     .arg(QString::number(snap.trueTrackDeg, 'f', 0))
		  << "<br/>";

		// Vertical rate — fpm, ze strzałką ↑↓→.
		const double vrFpm = snap.verticalRateMs / 0.00508;
		QString vrArrow;
		if (vrFpm >  100) vrArrow = "↑";
		else if (vrFpm < -100) vrArrow = "↓";
		else                   vrArrow = "→";
		s << QString(q_("Vertical rate: <b>%1 %2 fpm</b>"))
		     .arg(vrArrow)
		     .arg(QString::number(std::abs(vrFpm), 'f', 0))
		  << "<br/>";

		// Dystans poziomy do obserwatora (haversine).
		if (core)
		{
			const StelLocation& loc = core->getCurrentLocation();
			const double obsLat = loc.getLatitude();
			const double obsLon = loc.getLongitude();
			const double toRad  = M_PI / 180.0;
			const double dLat   = (snap.currentLat - obsLat) * toRad;
			const double dLon   = (snap.currentLon - obsLon) * toRad;
			const double a      = std::sin(dLat/2) * std::sin(dLat/2)
			                    + std::cos(obsLat * toRad) * std::cos(snap.currentLat * toRad)
			                    * std::sin(dLon/2) * std::sin(dLon/2);
			const double distKm = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a)) * 6371.0;
			s << QString(q_("Horizontal distance: <b>%1 km</b>"))
			     .arg(QString::number(distKm, 'f', 1))
			  << "<br/>";
		}
	}

	postProcessInfoString(str, flags);
	return str;
}
