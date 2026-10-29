--TEST--
XHProfRuns_Default: run files cannot instantiate objects
--SKIPIF--
<?php
// The Windows build copies the tests away from the rest of the repository.
if (!file_exists(dirname(__FILE__).'/../../xhprof_lib/utils/xhprof_runs.php')) {
    print 'skip xhprof_lib is not available next to the tests';
}
?>
--FILE--
<?php

include_once dirname(__FILE__).'/../../xhprof_lib/utils/xhprof_lib.php';
include_once dirname(__FILE__).'/../../xhprof_lib/utils/xhprof_runs.php';

class Gadget {
    public $x = 1;
    public function __wakeup() {
        echo "wakeup\n";
    }
}

$dir = sys_get_temp_dir() . '/xhprof_runs_002_' . getmypid();
mkdir($dir);

$runs = new XHProfRuns_Default($dir);

file_put_contents($dir . '/evil.ns.xhprof', serialize(array('main()' => new Gadget())));
$data = $runs->get_run('evil', 'ns', $desc);
var_dump(get_class($data['main()']));

$runs->save_run(array('main()' => array('ct' => 1, 'wt' => 2)), 'ns', 'good');
var_dump($runs->get_run('good', 'ns', $desc));

array_map('unlink', glob($dir . '/*'));
rmdir($dir);
?>
--EXPECT--
string(22) "__PHP_Incomplete_Class"
array(1) {
  ["main()"]=>
  array(2) {
    ["ct"]=>
    int(1)
    ["wt"]=>
    int(2)
  }
}
