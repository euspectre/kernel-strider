<$if callback.returnType$><$callback.returnType$><$else$>void<$endif$>
(*)(<$if concat(callback.arg.type)$><$callback.arg.type: join(, )$><$else$>void<$endif$>)