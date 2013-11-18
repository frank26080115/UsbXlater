#include "kbm2ctrl.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

// calculates both deadzone and keeps radius within limits to thwart anti-cheat detection
// also applies curves if a lookup table is supplied
void kbm2c_coordCalc(double* xptr, double* yptr, double gain, double dz, double lim, uint8_t* curve)
{
	double x = *xptr;
	double y = *yptr;

	double x2 = x * x;
	double y2 = y * y;

	double m;
	if (x == 0) {
		m = y;
	}
	else if (y == 0) {
		m = x;
	}
	else {
		m = sqrt(x2 + y2);
	}

	m *= gain;

	if (m < 0) m = -m;

	if (curve != 0)
	{
		double ml = m;
		if (ml > lim) ml = lim;
		int idx = lround(ml);
		if (idx > 130) idx = 130;
		uint8_t c = curve[idx];
		m = c;
	}

	if (m > 0) m += dz;

	if (m > lim) m = lim;

	double ang;
	if (x != 0)
	{
		double ang = atan(fabs(y / x));
		double nx = fabs(m * cos(ang));
		double ny = fabs(m * sin(ang));
		(*xptr) = (x > 0) ? nx : -nx;
		(*yptr) = (y > 0) ? ny : -ny;
	}
	else
	{
		(*xptr) = 0;
		(*yptr) = (y > 0) ? m : -m;
	}
}

// deprecated
void kbm2c_deadZoneCalc(double* xptr, double* yptr, double dz)
{
	double x = (*xptr);
	double y = (*yptr);

	if (x != 0 && y != 0)
	{
		double atanRes = atan(fabs(y / x));
		double dzx = fabs(dz * cos(atanRes));
		double dzy = fabs(dz * sin(atanRes));
		(*xptr) = (x > 0) ? (x + dzx) : (x - dzx);
		(*yptr) = (y > 0) ? (y + dzy) : (y - dzy);
	}
	else
	{
		(*xptr) = (x > 0) ? (x + dz) : ((x < 0) ? (x - dz) : (*xptr));
		(*yptr) = (y > 0) ? (y + dz) : ((y < 0) ? (y - dz) : (*yptr));
	}
}

void kbm2c_inactiveStickCalc(double* xptr, double* yptr, int8_t* randDeadZone, uint8_t dz, int16_t fillerX, int16_t fillerY)
{
	if ((*xptr) != 0 || (*yptr) != 0) return;

	if (fillerX != 0 || fillerY != 0)
	{
		(*xptr) = fillerX - 127;
		(*yptr) = fillerY - 127;
	}
	else
	{
		// it would be really easy to detect a cheater if their equipment always centered on 0 all the time
		// so we fill it with a random small number, and randomly change this number
		if ((rand() % 10) == 0 || randDeadZone[0] == 0) {
			randDeadZone[0] = -(dz / 2) + (rand() % (dz / 4));
		}
		if ((rand() % 10) == 0 || randDeadZone[1] == 0) {
			randDeadZone[1] = -(dz / 2) + (rand() % (dz / 4));
		}
		(*xptr) = randDeadZone[0];
		(*yptr) = randDeadZone[1];
	}
}
