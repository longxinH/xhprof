--TEST--
XHProf: Test no seg fault when ignored_functions is used
Author: chuso
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

class Bar {

    static function doSomething() {
        self::doSomethingElse();
    }

    private static function doSomethingElse() {
        return \explode('.', "");
    }
}

xhprof_enable(0, ['ignored_functions' => ['Bar::doSomething']]);
Bar::doSomething();
$output = xhprof_disable();
print_canonical($output);
--EXPECTF--
Bar::doSomethingElse==>explode          : ct=       1; wt=*;
main()                                  : ct=       1; wt=*;
main()==>Bar::doSomethingElse           : ct=       1; wt=*;
main()==>xhprof_disable                 : ct=       1; wt=*;