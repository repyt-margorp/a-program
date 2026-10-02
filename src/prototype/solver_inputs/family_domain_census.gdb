set pagination off
set $case = 0
break synthesis.c:10449
commands
silent
set $synthesis = &synthesis
set $jobs = synthesis.jobs.count
set $steps = synthesis.steps
continue
end
break synthesis.c:10463
commands
silent
set $case = $case + 1
printf "domain-census\t%d\t%lu\t%lu\n", $case, $synthesis->jobs.count - $jobs, $synthesis->steps - $steps
continue
end
run
