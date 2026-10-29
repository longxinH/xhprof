--TEST--
XHProf: Ignored functions keep recursion levels and never ignore the root symbol
--FILE--
<?php

function ign() { return 1; }
function a($n) { ign(); if ($n) a($n - 1); }
function f($n) { return $n ? g($n - 1) : 0; }
function g($n) { return f($n); }

// Recursion through an ignored function.
xhprof_enable(0, array('ignored_functions' => array('ign', 'g')));
a(2);
f(2);
$output = xhprof_disable();
ksort($output);
foreach ($output as $key => $stats) {
    echo $key, ' ct=', $stats['ct'], "\n";
}

// Ignoring the root symbol must not crash.
xhprof_enable(0, array('ignored_functions' => 'main()'));
a(0);
$output = xhprof_disable();
echo isset($output['main()']) ? "root kept\n" : "root lost\n";
?>
--EXPECT--
a==>a@1 ct=1
a@1==>a@2 ct=1
f==>f@1 ct=1
f@1==>f@2 ct=1
main() ct=1
main()==>a ct=1
main()==>f ct=1
main()==>xhprof_disable ct=1
root kept
