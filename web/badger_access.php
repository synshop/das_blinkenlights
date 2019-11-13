<?php

$user = trim(preg_replace("/[^a-zA-Z0-9_]/","",$_POST[user]));
$reader = trim(preg_replace("/[^0-9]/","",$_POST[reader]));
$color = trim(preg_replace("/[^#a-fA-F0-9,]/","",$_POST[color]));

print "$user:$reader:$color\n";

$data = "$user:$reader:$color\n";
file_put_contents('log.txt', $data, FILE_APPEND);

$colors = explode(",", $color);

$data = "";
foreach($colors as $key => $value)
{
  $data .= "$value\n";
}

file_put_contents('bl_semaphor/outfile.txt', $data);
chmod('bl_semaphor/outfile.txt', 0664);

exec("/usr/local/bin/bl_sigusr1");

?>
