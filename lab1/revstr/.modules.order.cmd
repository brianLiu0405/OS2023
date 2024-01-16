cmd_revstr/modules.order := {  :; } | awk '!x[$$0]++' - > revstr/modules.order
