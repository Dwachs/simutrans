#ifndef AMPHI_SEARCHER_H
#define AMPHI_SEARCHER_H

#include "../../../bauer/wegbauer.h"

/*
 * Searches a route from a water side factory to
 * an other factory. Switches only one times from water
 * to land.
 * @author gerw, dwachs
 * @date 5.7.2009
 */

class amphi_searcher_t : public wegbauer_t
{
public:
	amphi_searcher_t(karte_t *welt, spieler_t *spl) : wegbauer_t(welt, spl) {};
protected:
	virtual bool is_allowed_step( const grund_t *from, const grund_t *to, long *costs );
};

#endif /* AMPHI_SEARCHER_H */
