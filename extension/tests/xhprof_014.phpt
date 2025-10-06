--TEST--
XHProf: Test previously called function can be profiled
Author: chuso
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

class Bar {
    static function set() {
        \explode('.', "");
    }
}

Bar::set();
xhprof_enable();
Bar::set();
$output = xhprof_disable();
print_canonical($output);
--EXPECTF--
Bar::set==>explode                      : ct=       1; wt=*;
main()                                  : ct=       1; wt=*;
main()==>Bar::set                       : ct=       1; wt=*;
main()==>xhprof_disable                 : ct=       1; wt=*;