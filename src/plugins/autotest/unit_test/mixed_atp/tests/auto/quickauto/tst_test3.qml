// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
import QtQuick 2.0
import QtTest 1.0

TestCase {
    function test_bolle() {
        verify(true, "verifying true");
    }

    function test_str() {
        compare("hallo", "hallo");
    }
}

