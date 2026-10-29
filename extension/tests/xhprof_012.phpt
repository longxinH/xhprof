--TEST--
XHProf: Test Curl Additional Info
Author: longxinhui
--SKIPIF--
<?php if (!extension_loaded("curl")) print 'skip'; ?>
--INI--
xhprof.collect_additional_info = 1
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

xhprof_enable();
$ch = curl_init();
curl_setopt($ch, CURLOPT_URL, "https://www.php.net/");
curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
curl_exec($ch);
$output = xhprof_disable();
print_canonical($output);
echo "\n";

?>
--EXPECTF--
main()                                  : ct=       1; wt=*;
main()==>curl_exec#https://www.php.net/ : ct=       1; wt=*;
main()==>curl_getinfo                   : ct=       1; wt=*;
main()==>curl_init                      : ct=       1; wt=*;
main()==>curl_setopt                    : ct=       2; wt=*;
main()==>xhprof_disable                 : ct=       1; wt=*;
