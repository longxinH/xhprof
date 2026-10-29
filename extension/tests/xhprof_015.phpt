--TEST--
XHProf: Ignoring functions in one run does not corrupt later runs
--FILE--
<?php

function helper() { return 1; }
function a($n) { helper(); return $n <= 0 ? 0 : a($n - 1); }
function b($n) { return $n <= 0 ? 0 : b($n - 1); }

function selfkeys(array $d) {
    $bad = array();
    foreach (array_keys($d) as $k) {
        if (strpos($k, '==>') === false) continue;
        list($p, $c) = explode('==>', $k, 2);
        if ($p === $c) $bad[] = $k;
    }
    return $bad;
}

function run_profile($label, array $ignore, $fn) {
    xhprof_enable(0, array('ignored_functions' => $ignore));
    $fn();
    $bad = selfkeys(xhprof_disable());
    printf("%s: %s\n", $label, $bad ? implode(', ', $bad) : 'ok');
}

run_profile('clean', array(), function () { a(3); b(3); });
run_profile('ignore helper', array('helper'), function () { a(0); });
run_profile('clean again', array(), function () { a(3); b(3); });
run_profile('clean once more', array(), function () { a(3); b(3); });
?>
--EXPECT--
clean: ok
ignore helper: ok
clean again: ok
clean once more: ok
