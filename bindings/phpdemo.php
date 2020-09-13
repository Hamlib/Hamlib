<?php
// This example needs to be improved by adding get_freq, set_freq functions and such.
// Need to parse the returns from rigctl
print("Hamlib demo\n");
$descriptorspec = array(
   0 => array("pipe", "r"),  // stdin is a pipe that the child will read from
   1 => array("pipe", "w"),  // stdout is a pipe that the child will write to
   2 => array("file", "/tmp/error-output.txt", "a") // stderr is a file to write to
);

$cmd = "rigctl -m 2";
$process = proc_open($cmd, $descriptorspec, $pipes);

if (is_resource($process)) {
    // $pipes now looks like this:
    // 0 => writeable handle connected to child stdin
    // 1 => readable handle connected to child stdout
    // Any error output will be appended to /tmp/error-output.txt

    echo fread($pipes[1], 32);

    echo "Main Freq: ";
    fwrite($pipes[0], "f Main\n");
    fread($pipes[1],32);
    echo fread($pipes[1],64);
    echo "\n";
    echo "=====\n";

    fwrite($pipes[0], "f Sub\n");
    echo "Sub  Freq:" ;
    echo fread($pipes[1],32);
    echo "\n";

    fclose($pipes[0]);
    fclose($pipes[1]);

    $return_value = proc_close($process);

    echo "command returned $return_value\n";
}
?>
