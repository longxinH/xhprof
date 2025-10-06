--TEST--
XHProf: Test main remains the root node
Author: chuso
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

class Bar {
    static function startProfiling() {
        xhprof_enable();
    }

    static function doSomething() {
        return \explode('.', "");
    }

    static function stopProfiling() {
        return xhprof_disable();
    }
}

function start() {
    Bar::startProfiling();
    Bar::doSomething();
    $output = Bar::stopProfiling();
    print_canonical($output);
}

start();
--EXPECTF--
Bar::doSomething==>explode              : ct=       1; wt=*;
Bar::stopProfiling==>xhprof_disable     : ct=       1; wt=*;
main()                                  : ct=       1; wt=*;
main()==>Bar::doSomething               : ct=       1; wt=*;
main()==>Bar::stopProfiling             : ct=       1; wt=*;