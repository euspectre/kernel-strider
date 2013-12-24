	/* Setting handlers for <$function.name$> */ {
		struct kedr_annotation *ann =
			kedr_get_annotation(<$function.index$>);
		BUG_ON(ann == NULL);
		handlers_<$function.name$>.pre = ann->pre;
		handlers_<$function.name$>.post = ann->post;
	}