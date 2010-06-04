#ifndef WATER_DIGGER_H
#define WATER_DIGGER_H

#include "../../../bauer/wegbauer.h"

/*
 * Searches a water route
 * may use terraforming to transform to water-level
 * @author dwachs
 */

class water_digger_t : public wegbauer_t
{
public:
	water_digger_t(karte_t *welt, spieler_t *spl) : wegbauer_t(welt, spl) {};

	bool terraform();
protected:
	virtual bool is_allowed_step( const grund_t *from, const grund_t *to, long *costs );
};

#endif /* WATER_DIGGER_H */
