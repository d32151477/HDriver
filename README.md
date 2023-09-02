# HDriver
프로세스 물리 메모리 주소를 읽고 쓰는 윈도우 커널 드라이버입니다.

## IOCTL 코드
해당 IOCTL 코드를 통해 커널 드라이버와 통신할 수 있습니다. [DeviceIoControl]

IOCTL_SET_PID ```0x9c402418```

IOCTL_SET_ADRESS ```0x9c40241c```

IOCTL_READ_MEMORY ```0x9c402410```

IOCTL_WRITE_MEMORY ```0x9c402414```


## 설치 방법
- Windows SDK가 WDK (윈도우 드라이버 키트)가 설치되어 있어야 합니다.
  
- 프로젝트를 빌드합니다.

- 서명되지 않은 드라이버를 불러오기 위해 부트 디버그 옵션을 변경합니다.
  ```
  bcdedit /bootdebug on
  bcdedit /set nointegritychecks ON
  ```

  부트 디버그 옵션을 변경하지 않고 사용하려면 [EfiGuard]또는 [DSEFix] 등 을 사용하세요.

- 서비스 등록
  
  ```sc create HDriver binpath= "주소/HDriver.sys" start= demand type= kernel```

- 서비스 실행
  
  ```sc start HDriver```
  
[EfiGuard]: https://github.com/Mattiwatti/EfiGuard
[DSEFix]: https://github.com/hfiref0x/DSEFix
[DeviceIoControl]: https://learn.microsoft.com/ko-kr/windows/win32/api/ioapiset/nf-ioapiset-deviceiocontrol
