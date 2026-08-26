package com.example.demo.navigation;

public class ManualControlActiveException extends RuntimeException {
    public ManualControlActiveException() {
        super("관리자가 수동 운행 중이므로 목적지를 선택할 수 없습니다");
    }
}
