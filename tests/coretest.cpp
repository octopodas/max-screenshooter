#include "annotation.h"
#include "geometry.h"
#include "output.h"

#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

using namespace Geometry;

class CoreTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void rectFromPointsIncludesBothCorners()
    {
        QCOMPARE(rectFromPoints(QPoint(5, 7), QPoint(2, 3)), QRect(2, 3, 4, 5));
        QCOMPARE(rectFromPoints(QPoint(4, 4), QPoint(4, 4)), QRect(4, 4, 1, 1));
    }

    void hitTestFindsHandlesAndInside()
    {
        const QRect r(10, 10, 100, 50);
        QCOMPARE(hitTest(r, QPoint(10, 10), 10), Top | Left);
        QCOMPARE(hitTest(r, QPoint(61, 12), 10), int(Top));
        QCOMPARE(hitTest(r, QPoint(110, 35), 10), int(Right));
        QCOMPARE(hitTest(r, QPoint(40, 30), 10), int(Inside));
        QCOMPARE(hitTest(r, QPoint(300, 300), 10), int(NoHandle));
        QCOMPARE(hitTest(QRect(), QPoint(0, 0), 10), int(NoHandle));
        // On a tiny selection every handle overlaps; the corner must win.
        QCOMPARE(hitTest(QRect(10, 10, 4, 4), QPoint(10, 10), 10), Top | Left);
    }

    void resizeFlipsHandleAcrossOppositeEdge()
    {
        int handle = Right;
        const QRect r = resize(QRect(10, 10, 100, 50), handle, QPoint(0, 20));
        QCOMPARE(r, QRect(QPoint(0, 10), QPoint(10, 59)));
        QCOMPARE(handle, int(Left));

        handle = Bottom | Right;
        QCOMPARE(resize(QRect(10, 10, 100, 50), handle, QPoint(200, 100)), QRect(QPoint(10, 10), QPoint(200, 100)));
        QCOMPARE(handle, Bottom | Right);
    }

    void clampIntoKeepsSizeWhenItFits()
    {
        const QRect bounds(0, 0, 100, 100);
        QCOMPARE(clampInto(QRect(90, 90, 20, 20), bounds), QRect(80, 80, 20, 20));
        QCOMPARE(clampInto(QRect(-5, 0, 200, 10), bounds), QRect(0, 0, 100, 10));
    }

    void placeBesidePrefersBelowThenAboveThenInside()
    {
        const QRect bounds(0, 0, 1000, 1000);
        const QSize size(100, 30);
        QCOMPARE(placeBeside(QRect(100, 100, 200, 100), size, bounds, 8).topLeft(), QPoint(149, 208));
        QCOMPARE(placeBeside(QRect(100, 950, 200, 40), size, bounds, 8).bottom(), 941);
        QCOMPARE(placeBeside(bounds, size, bounds, 8).bottom(), 991);
        // Horizontal clamping near the left edge.
        QCOMPARE(placeBeside(QRect(0, 100, 20, 20), size, bounds, 8).left(), 0);
    }

    void scaleAroundKeepsAnchorFixed()
    {
        QCOMPARE(scaleAround(QPoint(100, 100), QPoint(150, 150), 2.0), QPoint(50, 50));
        QCOMPARE(scaleAround(QPoint(100, 100), QPoint(150, 150), 0.5), QPoint(125, 125));
        QCOMPARE(scaleAround(QPoint(100, 100), QPoint(100, 100), 3.0), QPoint(100, 100));
    }

    void stepZoomWalksPresetsAndClamps()
    {
        QCOMPARE(stepZoom(1.0, 1), 1.25);
        QCOMPARE(stepZoom(1.0, -1), 0.75);
        QCOMPARE(stepZoom(1.0, 2), 1.5);
        QCOMPARE(stepZoom(8.0, 1), 8.0);
        QCOMPARE(stepZoom(0.1, -1), 0.1);
        // A zoom between presets moves to the neighbouring preset.
        QCOMPARE(stepZoom(0.9, 1), 1.0);
        QCOMPARE(stepZoom(0.9, -1), 0.75);
    }

    void timestampedNameFormat()
    {
        const QDateTime time(QDate(2026, 10, 29), QTime(14, 3, 22));
        QCOMPARE(Output::timestampedName(time), QStringLiteral("Screenshot_2026-10-29_14-03-22"));
    }

    void uniquePathSkipsExistingFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QCOMPARE(Output::uniquePath(dir.path(), QStringLiteral("a"), QStringLiteral("png")), dir.filePath(QStringLiteral("a.png")));
        QFile(dir.filePath(QStringLiteral("a.png"))).open(QIODevice::WriteOnly);
        QCOMPARE(Output::uniquePath(dir.path(), QStringLiteral("a"), QStringLiteral("png")), dir.filePath(QStringLiteral("a_2.png")));
    }

    void stripAddedExtensionUndoesKdeAutoExtension()
    {
        QCOMPARE(Output::stripAddedExtension(QStringLiteral("/a/shot.jpg.png")), QStringLiteral("/a/shot.jpg"));
        QCOMPARE(Output::stripAddedExtension(QStringLiteral("/a/shot.WEBP.png")), QStringLiteral("/a/shot.WEBP"));
        QCOMPARE(Output::stripAddedExtension(QStringLiteral("/a/shot.png.jpg")), QStringLiteral("/a/shot.png"));
        QCOMPARE(Output::stripAddedExtension(QStringLiteral("/a/shot.png")), QStringLiteral("/a/shot.png"));
        QCOMPARE(Output::stripAddedExtension(QStringLiteral("/a/v1.2.png")), QStringLiteral("/a/v1.2.png"));
    }

    void ocrLanguagePrefersConfiguredThenEnglishAndPolish()
    {
        const QStringList all = {QStringLiteral("deu"), QStringLiteral("eng"), QStringLiteral("pol")};
        QCOMPARE(Output::ocrLanguageArgument(QStringLiteral("deu"), all), QStringLiteral("deu"));
        QCOMPARE(Output::ocrLanguageArgument(QString(), all), QStringLiteral("eng+pol"));
        QCOMPARE(Output::ocrLanguageArgument(QString(), {QStringLiteral("rus"), QStringLiteral("eng")}), QStringLiteral("eng+rus"));
        QCOMPARE(Output::ocrLanguageArgument(QString(), {QStringLiteral("eng")}), QStringLiteral("eng"));
        QCOMPARE(Output::ocrLanguageArgument(QString(), {QStringLiteral("deu")}), QStringLiteral("deu"));
    }

    void pixelateMakesUniformBlocks()
    {
        QImage image(40, 40, QImage::Format_RGB32);
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                image.setPixel(x, y, qRgb(x * 6, y * 6, 0));
            }
        }
        Annotations::pixelate(image, QRect(0, 0, 40, 40), 10);
        for (int by = 0; by < 4; ++by) {
            for (int bx = 0; bx < 4; ++bx) {
                const QRgb first = image.pixel(bx * 10, by * 10);
                for (int y = 0; y < 10; ++y) {
                    for (int x = 0; x < 10; ++x) {
                        QCOMPARE(image.pixel(bx * 10 + x, by * 10 + y), first);
                    }
                }
            }
        }
        QVERIFY(image.pixel(0, 0) != image.pixel(39, 39));
        // A rectangle reaching past the image is clipped, not a crash.
        Annotations::pixelate(image, QRect(30, 30, 50, 50), 10);
    }

    void strayClicksAreNotMeaningful()
    {
        Annotation a;
        a.tool = Tool::Arrow;
        a.from = a.to = QPoint(5, 5);
        QVERIFY(!a.isMeaningful());
        a.to = QPoint(30, 5);
        QVERIFY(a.isMeaningful());

        a.tool = Tool::Rectangle;
        a.to = QPoint(6, 6);
        QVERIFY(!a.isMeaningful());

        a.tool = Tool::Text;
        a.text = QStringLiteral("  ");
        QVERIFY(!a.isMeaningful());
        a.text = QStringLiteral("hi");
        QVERIFY(a.isMeaningful());
    }
};

QTEST_MAIN(CoreTest)
#include "coretest.moc"
