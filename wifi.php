<?php
if(sizeof($_GET) < 1){
        echo time();
        exit();
}

if(isset($_GET['file'])){
        $max = 8192000;//~8mb in characters
        if(is_numeric($_GET['file'])){
                $max = $_GET['file'];
        }
        header("Content-type: text/plain");
        echo "START\n";
        $pool = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        for($i=0; $i<$max; $i++){
                echo $pool[rand(0, 62)];
        }
}else if(isset($_GET['concurrent'])){
        $_GET['server_time'] = time();
        file_put_contents('/home/ubuntu/phil/concurrent_wifi_results.txt', json_encode($_GET)."\n", FILE_APPEND);
        echo "Fin\r\n";
}else if(isset($_GET['testplan'])){
        echo "2;1;15;128;512\n";
        //give test plan (TESTNUMBER;<File(1) | Stream(2)>;WAIT/STREAM_TIME;<MINSIZE/BITRATEinKB/Kbps>;<MAX_Size/Bitrate_InK>)
        //max wait is 90 seconds below
        //example: 1;1;90;128; does test with all units submitting 128kbps every 30s-2m
        //example: 2;2;90;512;1500 does test with streaming randomly between 512 and 1500kbps
        //example: 3;1;90;128;512 does test with file downloads of random sizes between 128 and 512kbps, weighing 128kb files more often

}else{
        $_GET['server_time'] = time();
        file_put_contents('/home/ubuntu/phil/wifi_results.txt', json_encode($_GET)."\n", FILE_APPEND);
        echo "Fin\r\n";
}
?>
