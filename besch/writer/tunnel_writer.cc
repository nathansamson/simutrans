#include "../../utils/cstring_t.h"
#include "../../dataobj/tabfile.h"
#include "../../dataobj/ribi.h"
#include "../tunnel_besch.h"
#include "obj_node.h"
#include "text_writer.h"
#include "xref_writer.h"
#include "imagelist_writer.h"
#include "skin_writer.h"
#include "get_waytype.h"
#include "tunnel_writer.h"


void tunnel_writer_t::write_obj(FILE* fp, obj_node_t& parent, tabfileobj_t& obj)
{
	int pos, i;

	obj_node_t node(this, 21, &parent);

	uint32 topspeed    = obj.get_int("topspeed",     999);
	uint32 preis       = obj.get_int("cost",           0);
	uint32 maintenance = obj.get_int("maintenance", 1000);
	uint8 wegtyp       = get_waytype(obj.get("waytype"));

	// prissi: timeline
	uint16 intro_date  = obj.get_int("intro_year", DEFAULT_INTRO_DATE) * 12;
	intro_date += obj.get_int("intro_month", 1) - 1;

	uint16 obsolete_date  = obj.get_int("retire_year", DEFAULT_RETIRE_DATE) * 12;
	obsolete_date += obj.get_int("retire_month", 1) - 1;

	// Version uses always high bit set as trigger
	// version 2: snow images
	uint16 version = 0x8003;
	node.write_uint16(fp, version,        0);
	node.write_uint32(fp, topspeed,       2);
	node.write_uint32(fp, preis,          6);
	node.write_uint32(fp, maintenance,   10);
	node.write_uint8 (fp, wegtyp,        14);
	node.write_uint16(fp, intro_date,    15);
	node.write_uint16(fp, obsolete_date, 17);

	sint8 number_seasons = 0;

	static const char* const indices[] = { "n", "s", "e", "w" };
	slist_tpl<cstring_t> backkeys;
	slist_tpl<cstring_t> frontkeys;

	slist_tpl<cstring_t> cursorkeys;
	cursorkeys.append(cstring_t(obj.get("cursor")));
	cursorkeys.append(cstring_t(obj.get("icon")));

	char buf[40];
	sprintf(buf, "%simage[%s][0]", "back", indices[0]);

	cstring_t str = obj.get(buf);
	if (strlen(str) == 0) {
		node.write_sint8(fp, number_seasons, 19);
		write_head(fp, node, obj);

		for (pos = 0; pos < 2; pos++) {
			for (i = 0; i < 4; i++) {
				sprintf(buf, "%simage[%s]", pos ? "back" : "front", indices[i]);
				cstring_t str = obj.get(buf);
				(pos ? &backkeys : &frontkeys)->append(str);
			}
		}
		imagelist_writer_t::instance()->write_obj(fp, node, backkeys);
		imagelist_writer_t::instance()->write_obj(fp, node, frontkeys);
		backkeys.clear();
		frontkeys.clear();
		cursorskin_writer_t::instance()->write_obj(fp, node, obj, cursorkeys);
	} else {
		while(number_seasons < 2) {
			sprintf(buf, "%simage[%s][%d]", "back", indices[0], number_seasons+1);
			cstring_t str = obj.get(buf);
			if(strlen(str) > 0) {
				number_seasons++;
			} else {
				break;
			}
		}
		node.write_sint8(fp, number_seasons, 19);
		write_head(fp, node, obj);

		for (uint8 season = 0; season <= number_seasons ; season++) {
			for (pos = 0; pos < 2; pos++) {
				for (i = 0; i < 4; i++) {
					sprintf(buf, "%simage[%s][%d]", pos ? "back" : "front", indices[i], season);
					cstring_t str = obj.get(buf);
					(pos ? &backkeys : &frontkeys)->append(str);
				}
			}
			imagelist_writer_t::instance()->write_obj(fp, node, backkeys);
			imagelist_writer_t::instance()->write_obj(fp, node, frontkeys);
			backkeys.clear();
			frontkeys.clear();
			if(season == 0) {
				cursorskin_writer_t::instance()->write_obj(fp, node, obj, cursorkeys);
			}
		}
	}
	str = obj.get("way");
	if (str.len() > 0) {
		xref_writer_t::instance()->write_obj(fp, node, obj_way, str, true);
		node.write_sint8(fp, 1, 20);
	}
	else {
		node.write_sint8(fp, 0, 20);
	}

	cursorkeys.clear();

	node.write(fp);
}
