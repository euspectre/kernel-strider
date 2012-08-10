<$if is_external$>(call_info->op_orig \
? (unsigned long)call_info->op_orig : (unsigned long)call_info->return_address)<$else$>((unsigned long)call_info->op_orig)<$endif$>