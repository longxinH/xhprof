--TEST--
XHProf: Test ignored functions
Author: chuso
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

// 1: Test ignored internal functions
class Bar {
    static function set() {
        \explode('.', "");
    }
}

xhprof_enable(0, ['ignored_functions' => ['explode']]);
Bar::set();
$output = xhprof_disable();
echo "Part 1: Ignored internal function\n";
print_canonical($output);

function profileCallback(callable $callback) {
    $callback();
}

// 2: Test ignored callback function

$callback = function(): void {};
xhprof_enable(0, ['ignored_functions' => ['profileCallback']]);
profileCallback($callback);
$output = xhprof_disable();
echo "Part 2: Ignored callback function\n";
print_canonical($output);

--EXPECTF--
Part 1: Ignored internal function
main()                                  : ct=       1; wt=*;
main()==>Bar::set                       : ct=       1; wt=*;
main()==>xhprof_disable                 : ct=       1; wt=*;
Part 2: Ignored callback function
main()                                  : ct=       1; wt=*;
main()==>xhprof_disable                 : ct=       1; wt=*;
main()==>{closure}                      : ct=       1; wt=*;