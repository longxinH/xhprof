--TEST--
XHProf: Test no memory leak
Author: chuso
--FILE--
<?php
// array_filter_short.phpº
namespace Somewhere\On\Earth;
class Foo {
    function leaking(array $array) {
        $f = function ($array) {
            return \array_filter($array, function($item) { return !is_null($item); });
        };
        $clone = (array) $array;
        $res = $f($clone);
        unset($clone, $res);
        $previous_memory = null;
        for ($i = 0; $i < 1000; $i++) {
            $clone = (array) $array;
            $res = $f($clone);
            $memory = \memory_get_usage();
            if ($previous_memory !== null && $previous_memory !== $memory) {
                $diff = $memory - $previous_memory;
                echo "Memory leak detected: {$diff} bytes\n";
            }
            $previous_memory = $memory;
            unset($clone, $res);
        }
        echo "No memory leak detected\n";
    }
    function run() {
        $array = array_combine(range(1, 1000, 2), array_fill(1, 500, null));
        $array += array_combine(range(2, 1000, 2), array_fill(1, 500, true));
        $this->leaking($array);
    }
}
echo "Test no memory leak\n";
(new Foo())->run();

// run with php80 -n -dzend_extension=opcache -dopcache.enable=1 -dopcache.enable_cli=1 -dextension=xhprof.so array_filter_short.php
--EXPECTF--
Test no memory leak
No memory leak detected