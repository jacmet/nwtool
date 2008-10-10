#ifndef _NWTOOL_H_
#define _NWTOOL_H_

#define NW_MODE_MOUSE 0
#define NW_MODE_DIGITIZER 1
#define NW_MODE_MULTITOUCH 2

struct nw;

struct nw_ops {
	int (*get_info) (struct nw *);
	int (*calibrate) (struct nw *, int enable);
	int (*set_mode) (struct nw *, int mode);
};

struct nw {
	/* serial number, model, whatever
	   maybe type */
	int debug_level;
	struct nw_ops *ops;
};

#endif
