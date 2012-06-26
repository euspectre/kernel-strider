<$if concat(subevent.args)$>
generate_event_begin_<$event.type$>(<$event.args$>, <$subevent_count: join( + )$>);
<$generate_subevent: join(\n)$>
generate_event_end_<$event.type$>();
<$else$>
generate_event_<$event.type$>(<$event.args$>);
<$endif$>