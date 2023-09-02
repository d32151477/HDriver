# HDriver
프로세스 물리 메모리 주소를 읽고 쓰는 윈도우 커널 드라이버입니다.

# 설치
- Windows SDK가 WDK (윈도우 드라이버 키트) 설치되어있어야합니다.
  
- 프로젝트를 빌드합니다.

- 서명되지 않은 드라이버를 불러오기 위해 부트 디버그 옵션을 변경합니다.
  ```
  bcdedit /bootdebug on
  bcdedit /set nointegritychecks ON
  ```

  부트 디버그 옵션을 변경하지 않고 사용하려면 우회 프로그램 [EfiGuard]또는 [DSEFix] 등 을 사용하세요.

[EfiGuard]: https://github.com/Mattiwatti/EfiGuard
[DSEFix]: https://github.com/hfiref0x/DSEFix
