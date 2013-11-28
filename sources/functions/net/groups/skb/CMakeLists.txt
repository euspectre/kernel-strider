# This group handles (part of) allocation and deallocation operations with
# socket buffers, SKBs.
# For the present, only allocation and deallocation of struct skbuff is
# processed here but not the lifetime of the SKB data referred to by the
# struct (no tracking of fragments, etc.). This makes the model manageable
# albeit incomplete.

kmodule_configure_kernel_functions(functions OPTIONAL
	# alloc
	"__alloc_skb"
	"build_skb"
	"__netdev_alloc_skb"
	"skb_clone"
	"skb_copy"
	"__pskb_copy"
	"pskb_copy"
	"skb_realloc_headroom"
	"skb_copy_expand"
	"dev_alloc_skb"

	# free and free-like operations
	"__kfree_skb"
	"kfree_skb"
	"consume_skb"
	"skb_queue_purge"
	"dev_kfree_skb_irq"
	"dev_kfree_skb_any"

	# operations that also should be handled as free()
	"dev_queue_xmit"  # SKB is consumed there
	"netif_receive_skb" # SKB is passed up the network stack
	"netif_rx"
	"napi_gro_receive"
)
group_create_handlers(${GROUP_NAME} ${functions})

group_add_sources("handlers_${GROUP_NAME}.c")
############################################################################
