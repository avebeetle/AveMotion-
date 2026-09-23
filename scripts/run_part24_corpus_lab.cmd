@echo off
chcp 65001 >nul
setlocal
python "%~dp0run_part24_corpus_lab.py" %*
exit /b %ERRORLEVEL%
