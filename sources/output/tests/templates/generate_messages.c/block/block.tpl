<$if concat(submessage.args)$>
generate_message_begin_<$message.type$>(<$message.args$>, <$submessage_count: join( + )$>);
<$generate_submessage: join(\n)$>
generate_message_end_<$message.type$>();
<$else$>
generate_message_<$message.type$>(<$message.args$>);
<$endif$>