#ifndef DATABLOCK_H
#define DATABLOCK_H

#include "../../linehandle_t.h"
#include "../../dataobj/koord3d.h"
#include "../../tpl/vector_tpl.h"
/*
 * This class contains just data.
 * The meaning of each entry depend on the
 * child which has created it.
 * @author gerw, dwachs
 * @date 12.7.09
 */

class datablock_t
{
public:
	vector_tpl<koord3d> pos1, pos2;
	linehandle_t line;
};

#endif /* DATABLOCK_H */