<?php
/**
 * 伺服器端機密組態範本：
 * 請複製為 config.php 並填入隨機產生的 Bearer Token。
 * 產生指令：python -c "import secrets; print(secrets.token_urlsafe(32))"
 * 注意：config.php 已被 .gitignore 排除，且受 .htaccess 保護禁止外部直接存取。
 */
return [
    'api_token' => 'REPLACE_WITH_RANDOM_TOKEN',
];

