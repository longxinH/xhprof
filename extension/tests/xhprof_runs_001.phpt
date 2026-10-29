--TEST--
XHProfRuns_Default: run ids and namespaces cannot escape the output directory
--SKIPIF--
<?php
// The Windows build copies the tests away from the rest of the repository.
if (!file_exists(dirname(__FILE__).'/../../xhprof_lib/utils/xhprof_runs.php')) {
    print 'skip xhprof_lib is not available next to the tests';
}
?>
--FILE--
<?php

// Keep the error messages out of the test output.
function xhprof_error($message) {}

include_once dirname(__FILE__).'/../../xhprof_lib/utils/xhprof_lib.php';
include_once dirname(__FILE__).'/../../xhprof_lib/utils/xhprof_runs.php';

$base = sys_get_temp_dir() . '/xhprof_runs_001_' . getmypid();
$dir = $base . '/runs';
mkdir($dir, 0777, true);

// A valid run sitting outside of the output directory.
file_put_contents($base . '/outside.ns.xhprof', serialize(array('main()' => array('ct' => 1))));

$runs = new XHProfRuns_Default($dir);

$id = $runs->save_run(array('main()' => array('ct' => 1)), 'ns', 'abc-1.2_x');
var_dump($id);
var_dump(is_array($runs->get_run('abc-1.2_x', 'ns', $desc)));

var_dump($runs->get_run('../outside', 'ns', $desc));
var_dump($runs->get_run('outside', 'ns/../../x', $desc));
var_dump($runs->get_run('..\\outside', 'ns', $desc));
var_dump($runs->get_run("abc-1.2_x\0", 'ns', $desc));
var_dump($runs->get_run('', 'ns', $desc));

var_dump($runs->save_run(array(), 'ns', '../escaped'));
var_dump($runs->save_run(array(), '../ns', 'id'));
var_dump(file_exists($base . '/escaped.ns.xhprof'));

array_map('unlink', glob($dir . '/*'));
unlink($base . '/outside.ns.xhprof');
rmdir($dir);
rmdir($base);
?>
--EXPECT--
string(9) "abc-1.2_x"
bool(true)
NULL
NULL
NULL
NULL
NULL
NULL
NULL
bool(false)
