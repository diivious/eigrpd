#ifndef EIGRP_TEST_ZEBRA_H
#define EIGRP_TEST_ZEBRA_H
#define _GNU_SOURCE 1
#include <assert.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define __packed __attribute__((packed))
#define frr_weak __attribute__((weak))
#define PRINTFRR(a,b)
#define XREF_INIT(a,b,c) NULL
#define XREF_LINK(x) do { (void)(x); } while (0)
#define array_size(x) (sizeof(x) / sizeof((x)[0]))
#define CHECK_FLAG(V,F) (((V) & (F)) != 0)
#define SET_FLAG(V,F) ((V) |= (F))
#define UNSET_FLAG(V,F) ((V) &= ~(F))
#define IPV4_MAX_BYTELEN 4
#define IPV6_MAX_BYTELEN 16
#define PREFIX_STRLEN 128
#define VRF_NAMSIZ 64
#define INTERFACE_NAMSIZ 64
#define VRF_DEFAULT 0
#define VRF_DEFAULT_NAME "default"
#define ROUTER_STR "Enable a routing process\n"
#define EIGRP_STR "EIGRP information\n"
#define AS_STR "Autonomous system number\n"
#define VRF_CMD_HELP_STR "VRF name\n"
#define NO_STR "Negate a command or set its defaults\n"
#define SHOW_STR "Show running system information\n"
#define CLEAR_STR "Reset functions\n"
#define DEBUG_STR "Debugging functions\n"
#define UNDEBUG_STR "Disable debugging functions\n"
#define IFNAME_STR "Interface name\n"
#define FRR_REDIST_STR_EIGRPD "<connected|static|kernel>"
#define REDIST_STR "Redistribute information from another routing protocol\n"
#define FRR_REDIST_HELP_STR_EIGRPD "Connected\nStatic\nKernel\n"
#define VTY_NEWLINE "\n"
#define VTY_CURR_XPATH (vty ? vty->xpath : "")
#define XPATH_MAXLEN 1024
#define ZEBRA_ROUTE_MAX 256
#define ZEBRA_ROUTE_EIGRP 88
#define ZEBRA_ROUTE_CONNECT 1
#define ZEBRA_ROUTE_STATIC 2
#define ZEBRA_ROUTE_KERNEL 3
#define ZEBRA_ROUTE_MAX 256
#define DPLANE_OP_ROUTE_INSTALL 1
#define DPLANE_OP_ROUTE_UPDATE 2
#define DPLANE_OP_ROUTE_DELETE 3
#define DPLANE_OP_NONE 0
#define DPLANE_OP_ROUTE_NOTIFY 4
#define DPLANE_OP_ROUTE_REMOVE 5
#define DPLANE_OP_ADDR_INSTALL 6
#define DPLANE_OP_ADDR_UNINSTALL 7
#define ZEBRA_FLAG_SELECTED 0x1
#define ZEBRA_FLAG_INTERNAL 0x2
#define RIB_SYSTEM_ROUTE 0x1
#define AFI_IP 1
#define AFI_IP6 2
#define SAFI_UNICAST 1
#define VTY_TERM 0
#define VTY_FILE 1
#define VTY_SHELL 2
#define LOG_INFO 6
#define LOG_DEBUG 7
#define CMD_ATTR_HIDDEN 0x01
#define CMD_ATTR_YANG 0x02
#define CMD_ATTR_NOSH 0x04
#define CMD_ATTR_DEPRECATED 0x08
#define XREFT_DEFUN 0
#define QOBJ_FIELDS int qobj_dummy
#define DECLARE_QOBJ_TYPE(x)
#define DECLARE_MGROUP(x)
#define DEFINE_MTYPE_STATIC(a,b,c)
#define DEFINE_HOOK(a,b,c)
#define FRR_DAEMON_INFO(name, inst)
#define FRR_DAEMON_INFO_VTY_PORT(name, inst, port)
#define FRR_NORETURN
#define FRR_UNUSED __attribute__((unused))

typedef unsigned int vrf_id_t;
typedef uint32_t ifindex_t;
typedef unsigned int route_tag_t;
typedef uint64_t route_value_t;

enum node_type { VIEW_NODE, ENABLE_NODE, CONFIG_NODE, INTERFACE_NODE, EIGRP_NODE, DEBUG_NODE };
struct candidate_config_stub { struct lyd_node *dnode; };
struct vty { int type; int node; char xpath[XPATH_MAXLEN]; void *index; void *index_sub; struct candidate_config_stub *candidate_config; };
struct cmd_token { const char *text; const char *arg; const char *varname; int type; };
struct cmd_element { const char *string; int (*func)(const struct cmd_element *, struct vty *, int, struct cmd_token *[]); const char *doc; int attr; int daemon; const char *name; void *xref; };
struct cmd_node { const char *name; enum node_type node; enum node_type parent_node; const char *prompt; int (*config_write)(struct vty *); int (*node_exit)(struct vty *); void *cmdgraph; void *cmd_vector; void *cmd_hash; bool graph_built; bool no_xpath; };
struct listnode { struct listnode *next; void *data; };
struct list { struct listnode *head; struct listnode *tail; unsigned int count; void (*del)(void *); };
struct prefix { uint8_t family; uint8_t prefixlen; union { struct in_addr prefix4; struct in6_addr prefix6; } u; };
struct prefix_ipv4 { uint8_t family; uint8_t prefixlen; struct in_addr prefix; };
struct route_table { struct route_node *top; };
struct route_node { struct route_node *next; struct route_node *parent; struct route_table *table; struct prefix p; void *info; unsigned int lock; };
struct event { int dummy; };
struct event_loop { int dummy; };
struct stream { unsigned char *data; size_t size; size_t endp; size_t getp; };
struct interface { char name[INTERFACE_NAMSIZ]; ifindex_t ifindex; unsigned int flags; unsigned int status; int operative; void *info; struct list *connected; uint32_t bandwidth; uint32_t mtu; vrf_id_t vrf_id; };
struct vrf { char name[VRF_NAMSIZ]; vrf_id_t vrf_id; };
struct connected { struct interface *ifp; struct prefix *address; struct prefix *destination; };
struct access_list { int dummy; };
struct prefix_list { int dummy; };
struct route_map { int dummy; };
struct distribute_ctx { int dummy; };
struct lyd_node { int dummy; };
struct nb_cb_create_args { const char *xpath; const struct lyd_node *dnode; };
struct nb_cb_modify_args { const char *xpath; const struct lyd_node *dnode; const char *value; };
struct nb_cb_destroy_args { const char *xpath; const struct lyd_node *dnode; };
struct nb_cb_apply_finish_args { const char *xpath; const struct lyd_node *dnode; };
struct frr_yang_module_info { const char *name; };
struct zebra_privs_t { int dummy; };
struct zclient { void (*zebra_connected)(struct zclient *); void *context; int sock; };
struct zapi_route { int type; int safi; int vrf_id; struct prefix prefix; uint8_t distance; uint32_t metric; uint32_t mtu; uint32_t tag; };
struct zapi_nexthop { int dummy; };
struct zebra_dplane_ctx { int dummy; };
union g_addr { struct in_addr ipv4; struct in6_addr ipv6; };
struct nexthop { int dummy; };
struct bfd_session_params { int dummy; };
struct keychain { int dummy; };
struct key { int index; char *string; };
struct message { int key; const char *str; };
typedef struct { uint32_t a[4]; } MD5_CTX;
typedef struct { uint32_t a[8]; } HMAC_SHA256_CTX;
static inline void MD5Init(MD5_CTX *ctx) { (void)ctx; }
static inline void MD5Update(MD5_CTX *ctx, const void *buf, size_t len) { (void)ctx; (void)buf; (void)len; }
static inline void MD5Final(unsigned char *digest, MD5_CTX *ctx) { (void)ctx; if (digest) memset(digest, 0, 16); }
static inline void HMAC__SHA256_Init(HMAC_SHA256_CTX *ctx, const void *key, size_t len) { (void)ctx; (void)key; (void)len; }
static inline void HMAC__SHA256_Update(HMAC_SHA256_CTX *ctx, const void *buf, size_t len) { (void)ctx; (void)buf; (void)len; }
static inline void HMAC__SHA256_Final(unsigned char *digest, HMAC_SHA256_CTX *ctx) { (void)ctx; if (digest) memset(digest, 0, 32); }
static inline struct keychain *keychain_lookup(const char *name) { (void)name; static struct keychain kc; return &kc; }
static inline struct key *key_lookup_for_send(struct keychain *kc) { (void)kc; static struct key k = { .index = 1, .string = "stub" }; return &k; }

#define ALL_LIST_ELEMENTS(list,node,nnode,datum) \
	(node) = ((list) ? (list)->head : NULL); \
	(node) && (((nnode) = (node)->next), ((datum) = (node)->data), 1); \
	(node) = (nnode)
#define ALL_LIST_ELEMENTS_RO(list,node,datum) \
	(node) = ((list) ? (list)->head : NULL); \
	(node) && (((datum) = (node)->data), 1); \
	(node) = (node)->next

#define listhead(L) ((L) ? (L)->head : NULL)
#define listnextnode(N) ((N) ? (N)->next : NULL)
#define listgetdata(N) ((N) ? (N)->data : NULL)
#define listcount(L) ((L) ? (L)->count : 0)

static inline struct list *list_new(void) { return calloc(1, sizeof(struct list)); }
static inline void list_delete(struct list **l) { if (l) { free(*l); *l = NULL; } }
static inline void listnode_add(struct list *l, void *data) { if (!l) return; struct listnode *n = calloc(1, sizeof(*n)); n->data=data; if (!l->head) l->head=n; else l->tail->next=n; l->tail=n; l->count++; }
static inline void listnode_delete(struct list *l, void *data) { (void)l; (void)data; }
static inline struct route_table *route_table_init(void) { return calloc(1, sizeof(struct route_table)); }
static inline void route_table_finish(struct route_table *t) { free(t); }
static inline struct route_node *route_node_get(struct route_table *t, const struct prefix *p) { (void)p; struct route_node *n=calloc(1,sizeof(*n)); n->table=t; if (p) n->p=*p; return n; }

static inline struct route_node *route_node_match(struct route_table *t, const struct prefix *p) { (void)t; (void)p; return NULL; }
static inline const char *ifindex2ifname(ifindex_t ifindex, vrf_id_t vrf_id) { (void)ifindex; (void)vrf_id; return "stub0"; }
static inline void vty_time_print(struct vty *vty, int seconds) { (void)vty; (void)seconds; }

static inline struct route_node *route_node_lookup(struct route_table *t, const struct prefix *p) { (void)t; (void)p; return NULL; }
static inline struct route_node *route_top(struct route_table *t) { return t ? t->top : NULL; }
static inline struct route_node *route_next(struct route_node *n) { return n ? n->next : NULL; }
static inline void route_unlock_node(struct route_node *n) { (void)n; }
static inline void route_lock_node(struct route_node *n) { (void)n; }

static inline struct stream *stream_new(size_t size) { struct stream *s=calloc(1,sizeof(*s)); s->data=calloc(1,size); s->size=size; return s; }
static inline void stream_free(struct stream *s) { if (s) { free(s->data); free(s); } }
static inline void stream_reset(struct stream *s) { if (s) s->endp=s->getp=0; }
static inline void stream_put(struct stream *s, const void *buf, size_t len) { if (!s || s->endp + len > s->size) return; memcpy(s->data+s->endp, buf, len); s->endp += len; }
static inline void stream_putc(struct stream *s, uint8_t v) { stream_put(s,&v,1); }
static inline void stream_putw(struct stream *s, uint16_t v) { uint16_t n=htons(v); stream_put(s,&n,2); }
static inline void stream_putl(struct stream *s, uint32_t v) { uint32_t n=htonl(v); stream_put(s,&n,4); }
static inline void stream_put_ipv4(struct stream *s, uint32_t v) { stream_put(s,&v,4); }
static inline void stream_get(void *dst, struct stream *s, size_t len) { if (!dst || !s) return; if (s->getp + len > s->endp) len = (s->endp > s->getp) ? s->endp - s->getp : 0; memcpy(dst, s->data + s->getp, len); s->getp += len; }
static inline uint8_t stream_getc(struct stream *s) { return (s && s->getp < s->endp) ? s->data[s->getp++] : 0; }
static inline uint16_t stream_getw(struct stream *s) { uint16_t v=0; if (s && s->getp+2<=s->endp) { memcpy(&v,s->data+s->getp,2); s->getp+=2; } return ntohs(v); }
static inline uint32_t stream_getl(struct stream *s) { uint32_t v=0; if (s && s->getp+4<=s->endp) { memcpy(&v,s->data+s->getp,4); s->getp+=4; } return ntohl(v); }
static inline uint32_t stream_get_ipv4(struct stream *s) { uint32_t v=0; if (s && s->getp+4<=s->endp) { memcpy(&v,s->data+s->getp,4); s->getp+=4; } return v; }
static inline size_t stream_get_endp(const struct stream *s) { return s ? s->endp : 0; }
static inline void stream_set_endp(struct stream *s, size_t p) { if (s) s->endp=p; }
static inline size_t stream_get_getp(const struct stream *s) { return s ? s->getp : 0; }
static inline void stream_set_getp(struct stream *s, size_t p) { if (s) s->getp=p; }
static inline unsigned char *STREAM_DATA(struct stream *s) { return s ? s->data : NULL; }
static inline size_t STREAM_WRITEABLE(struct stream *s) { return s && s->size>=s->endp ? s->size - s->endp : 0; }

static inline int vty_out(struct vty *vty, const char *fmt, ...) { (void)vty; (void)fmt; return 0; }
static inline int vty_out_newline(struct vty *vty, const char *fmt, ...) { (void)vty; (void)fmt; return 0; }

struct ip { unsigned int ip_hl:4, ip_v:4; uint8_t ip_tos; uint16_t ip_len; uint16_t ip_id; uint16_t ip_off; uint8_t ip_ttl; uint8_t ip_p; uint16_t ip_sum; struct in_addr ip_src; struct in_addr ip_dst; };
static inline int event_timer_remain_second(struct event *e) { (void)e; return 0; }
static inline const char *lookup_msg(const struct message *msg, int key, const char *def) { (void)msg; (void)key; return def ? def : "unknown"; }
static inline int argv_find(struct cmd_token **argv, int argc, const char *token, int *idx) { (void)argv; (void)argc; (void)token; if (idx) *idx = 0; return 0; }

static inline void zlog_debug(const char *fmt, ...) { (void)fmt; }
static inline void zlog_warn(const char *fmt, ...) { (void)fmt; }
static inline void zlog_err(const char *fmt, ...) { (void)fmt; }
static inline void flog_err(int code, const char *fmt, ...) { (void)code; (void)fmt; }
static inline void install_element(enum node_type node, const struct cmd_element *cmd) { (void)node; (void)cmd; }
static inline void install_node(struct cmd_node *node) { (void)node; }
static inline void install_default(enum node_type node) { (void)node; }
static inline void if_cmd_init_default(void) {}
static inline void vrf_cmd_init(void *x) { (void)x; }
static inline void VTY_PUSH_XPATH(enum node_type node, const char *xpath) { (void)node; (void)xpath; }
static inline int nb_cli_enqueue_change(struct vty *vty, const char *xpath, int op, const char *value) { (void)vty;(void)xpath;(void)op;(void)value; return 0; }
static inline int nb_cli_apply_changes(struct vty *vty, void *x) { (void)vty;(void)x; return 0; }
static inline int nb_cli_apply_changes_clear_pending(struct vty *vty, void *x) { (void)vty;(void)x; return 0; }
static inline void nb_cli_show_dnode_cmds(struct vty *vty, const struct lyd_node *dnode, bool defaults) { (void)vty;(void)dnode;(void)defaults; }
static inline const char *yang_dnode_get_string(const struct lyd_node *dnode, const char *xpath) { (void)dnode;(void)xpath; return "0"; }
static inline bool yang_dnode_exists(const struct lyd_node *dnode, const char *xpath) { (void)dnode;(void)xpath; return false; }
static inline const struct lyd_node *yang_dnode_get(const struct lyd_node *dnode, const char *xpath) { (void)dnode;(void)xpath; return NULL; }
static inline const char *lyd_get_value(const struct lyd_node *dnode) { (void)dnode; return ""; }
static inline const struct lyd_node *yang_dnode_getf(const struct lyd_node *dnode, const char *fmt, ...) { (void)dnode;(void)fmt; return NULL; }
static inline int str2prefix_ipv4(const char *s, struct prefix_ipv4 *p) { (void)s; if (p) { memset(p,0,sizeof(*p)); p->family=AF_INET; } return 1; }
static inline int str2prefix(const char *s, struct prefix *p) { (void)s; if (p) memset(p,0,sizeof(*p)); return 1; }
static inline const char *prefix2str(const struct prefix *p, char *buf, size_t len) { (void)p; if (buf && len) snprintf(buf,len,"0.0.0.0/0"); return buf; }
static inline struct vrf *vrf_lookup_by_name(const char *name) { static struct vrf v; snprintf(v.name,sizeof(v.name),"%s",name?name:VRF_DEFAULT_NAME); v.vrf_id=VRF_DEFAULT; return &v; }
static inline struct vrf *vrf_lookup_by_id(vrf_id_t id) { static struct vrf v; snprintf(v.name,sizeof(v.name),"%s",VRF_DEFAULT_NAME); v.vrf_id=id; return &v; }
static inline struct interface *if_lookup_by_name(const char *name, vrf_id_t vrf_id) { static struct interface ifp; (void)vrf_id; snprintf(ifp.name,sizeof(ifp.name),"%s",name?name:"stub0"); return &ifp; }
static inline struct interface *if_lookup_by_index(ifindex_t ifindex, vrf_id_t vrf_id) { static struct interface ifp; (void)ifindex;(void)vrf_id; snprintf(ifp.name,sizeof(ifp.name),"stub0"); return &ifp; }
static inline void if_add_hook(int type, int (*fn)(struct interface *)) { (void)type;(void)fn; }
static inline void if_del_hook(int type, int (*fn)(struct interface *)) { (void)type;(void)fn; }
static inline void *XCALLOC(int type, size_t size) { (void)type; return calloc(1,size); }
static inline void *XMALLOC(int type, size_t size) { (void)type; return malloc(size); }
static inline void XFREE(int type, void *ptr) { (void)type; free(ptr); }
static inline char *XSTRDUP(int type, const char *s) { (void)type; return s ? strdup(s) : NULL; }

#define CMD_SUCCESS 0
#define CMD_WARNING 1
#define CMD_WARNING_CONFIG_FAILED 13
#define CMD_ERR_NO_MATCH 2
#define CMD_ERR_NOTHING_TODO 6
#define NB_OP_CREATE 1
#define NB_OP_DESTROY 2
#define NB_OP_MODIFY 3
#define NB_OP_DELETE 4
#define NB_EV_VALIDATE 1
#define NB_EV_PREPARE 2
#define NB_EV_ABORT 3
#define NB_EV_APPLY 4
#define EIGRP_FERR_START 10000
#define END_FERR 0
struct log_ref { int code; const char *title; const char *description; const char *suggestion; };
static inline void log_ref_add(struct log_ref *refs) { (void)refs; }
#define MTYPE_EIGRP_AUTH_TLV 1001
#define MTYPE_EIGRP_AUTH_SHA256_TLV 1002
#define MTYPE_EIGRP_TOP_ENTRY 1003
#define MTYPE_EIGRP_TOP 1004
#define MTYPE_EIGRP_NEIGHBOR 1005
#define MTYPE_EIGRP_INTERFACE 1006
#define MTYPE_EIGRP_PACKET 1007
#define MTYPE_EIGRP_PACKET_QUEUE 1008
#define MTYPE_EIGRP_FIFO 1009
#define MTYPE_EIGRP 1010
#define WORD_TKN 1

extern struct running_config_stub { struct lyd_node *dnode; } *running_config;

#endif
