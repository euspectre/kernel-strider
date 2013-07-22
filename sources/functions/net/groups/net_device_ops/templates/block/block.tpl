static void
cb_<$function.name$>_pre(struct kedr_local_storage *ls)
{
	struct net_device *dev;
	unsigned long pc = ls->fi->addr;
<$if get_netdev$>
<$get_netdev$>
<$else$>
	dev = (struct net_device *)KEDR_LS_ARG1(ls);
<$endif$>
	/* Relation: start of register_netdev* HB start of the callback. */
	kedr_happens_after(ls->tid, pc, (unsigned long)dev);
<$if is_rtnl_locked$>
	kedr_rtnl_locked_start(ls, pc);
<$endif$><$if is_bh_disabled$>
	kedr_bh_disabled_start(ls->tid, pc);
<$endif$><$if code.pre$>
	/* Other relations and sync rules */ {
<$code.pre$>
	}
<$endif$>}

static void
cb_<$function.name$>_post(struct kedr_local_storage *ls)
{
	struct net_device *dev;
	unsigned long pc = ls->fi->addr;
<$if get_netdev$>
<$get_netdev$>
<$else$>
	dev = (struct net_device *)KEDR_LS_ARG1(ls);
<$endif$><$if code.post$>
	/* Other relations and sync rules */ {
<$code.post$>
	}
<$endif$><$if is_bh_disabled$>
	kedr_bh_disabled_end(ls->tid, pc);
<$endif$><$if is_rtnl_locked$>
	kedr_rtnl_locked_end(ls, pc);
<$endif$>
	/* Relation: end of the callback HB end of unregister_netdev*. */
	kedr_happens_before(ls->tid, pc, (unsigned long)dev + 1);
}