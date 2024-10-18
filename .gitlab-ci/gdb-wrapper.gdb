set charset UTF-8
set logging redirect on
set logging debugredirect on
set logging enabled on
set schedule-multiple on
set detach-on-fork off

catch signal
commands
  set logging enabled off
  py print ("# " + gdb.execute ("thread apply all bt full", True, True).replace ("\n", "\n# "))
  set logging enabled on
  c
end

catch signal SIGTRAP
commands
  set logging enabled off
  py print ("# " + gdb.execute ("thread apply all bt full", True, True).replace ("\n", "\n# "))
  set logging enabled on
  q 1
end

r
if $_isvoid($_exitcode)
  q $_exitsignal
else
  q $_exitcode
