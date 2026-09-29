@echo off
chcp 65001 >nul
echo =======================================
echo Локальное сохранение и отправка в Git
echo =======================================
echo.

:: Запрашиваем описание коммита
set /p commit_msg="Введите описание изменений (Enter = 'Обновление кода'): "
if "%commit_msg%"=="" set commit_msg=Обновление кода

echo.
echo [1/3] Добавление файлов...
git add .

echo [2/3] Сохранение коммита...
git commit -m "%commit_msg%"

echo.
:: Спрашиваем про отправку на сервер
set /p do_push="Отправить изменения на сервер Gitflic? (Y/N): "

:: Проверяем ответ (ключ /i игнорирует заглавные/строчные буквы)
if /i "%do_push%"=="Y" goto push_yes
if /i "%do_push%"=="Д" goto push_yes
if /i "%do_push%"=="y" goto push_yes

:: Если ввели N, Enter или что-то другое
echo.
echo [3/3] Пропуск. Изменения сохранены ТОЛЬКО на компьютере.
goto finish

:push_yes
echo.
echo [3/3] Отправка кода на сервер...
git push

:finish
echo.
echo =======================================
echo Готово! Можно закрывать окно.
pause