<$if trigger.add_before$><$trigger.add_before$>

<$endif$>static noinline void
trigger_<$function.name$>(void)
{
<$trigger.code$>
}