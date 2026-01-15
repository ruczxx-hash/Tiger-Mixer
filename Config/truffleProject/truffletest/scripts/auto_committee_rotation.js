#!/usr/bin/env node

/**
 * 自动委员会轮换脚本
 * 
 * 功能：每隔指定时间间隔自动执行委员会轮换
 * 使用方式：node scripts/auto_committee_rotation.js [间隔秒数]
 * 例如：node scripts/auto_committee_rotation.js 60  # 每60秒执行一次
 */

const { exec } = require('child_process');
const path = require('path');
const fs = require('fs');

// 配置
const ROTATION_INTERVAL = process.argv[2] ? parseInt(process.argv[2]) : 60; // 默认60秒
const LOG_DIR = path.join(__dirname, '../logs');
const LOG_FILE = path.join(LOG_DIR, 'auto_rotation.log');
const PID_FILE = path.join(LOG_DIR, 'auto_rotation.pid');
const ROTATION_SCRIPT = path.join(__dirname, 'update_and_rotate_new.js');

// 确保日志目录存在
if (!fs.existsSync(LOG_DIR)) {
    fs.mkdirSync(LOG_DIR, { recursive: true });
}

// 日志函数
function log(message, level = 'INFO') {
    const timestamp = new Date().toISOString();
    const logMessage = `[${timestamp}] [${level}] ${message}\n`;
    
    // 输出到控制台
    console.log(logMessage.trim());
    
    // 写入日志文件
    fs.appendFileSync(LOG_FILE, logMessage);
}

// 执行轮换的函数
function executeRotation() {
    return new Promise((resolve, reject) => {
        log('========================================');
        log('开始执行委员会轮换检查...');
        
        const startTime = Date.now();
        const command = `truffle exec "${ROTATION_SCRIPT}"`;
        
        exec(command, { 
            cwd: path.join(__dirname, '..'),
            maxBuffer: 10 * 1024 * 1024 // 10MB buffer
        }, (error, stdout, stderr) => {
            const duration = ((Date.now() - startTime) / 1000).toFixed(2);
            
            if (error) {
                log(`❌ 轮换执行失败 (耗时: ${duration}秒)`, 'ERROR');
                log(`错误信息: ${error.message}`, 'ERROR');
                if (stderr) {
                    log(`标准错误: ${stderr}`, 'ERROR');
                }
                reject(error);
                return;
            }
            
            // 记录输出（可选：只记录关键信息）
            const lines = stdout.split('\n');
            let successCount = 0;
            let errorCount = 0;
            let skipCount = 0;
            
            // 解析输出
            for (const line of lines) {
                if (line.includes('✅')) {
                    successCount++;
                } else if (line.includes('❌')) {
                    errorCount++;
                } else if (line.includes('⚠️') || line.includes('跳过')) {
                    skipCount++;
                }
                
                // 记录重要信息
                if (line.includes('新委员会成员') || 
                    line.includes('轮换成功') || 
                    line.includes('已写入新委员会') ||
                    line.includes('当前轮次') ||
                    line.includes('结果汇总')) {
                    log(line.trim());
                }
            }
            
            log(`✅ 轮换检查完成 (耗时: ${duration}秒, 成功:${successCount}, 错误:${errorCount}, 跳过:${skipCount})`);
            
            // 检查是否实际执行了轮换
            if (stdout.includes('委员会轮换成功') || stdout.includes('已写入新委员会成员到文件')) {
                log('🎉 委员会轮换已执行！', 'SUCCESS');
            } else if (stdout.includes('当前不可轮换') || stdout.includes('跳过轮换')) {
                log('⏭️  本次跳过轮换（条件未满足）', 'INFO');
            }
            
            log('========================================');
            resolve();
        });
    });
}

// 主循环
let rotationCount = 0;
let successCount = 0;
let failCount = 0;
let skipCount = 0;

async function mainLoop() {
    log('========================================');
    log('自动委员会轮换服务已启动');
    log(`轮换间隔: ${ROTATION_INTERVAL} 秒`);
    log(`日志文件: ${LOG_FILE}`);
    log(`PID 文件: ${PID_FILE}`);
    log(`轮换脚本: ${ROTATION_SCRIPT}`);
    log('========================================');
    
    // 写入 PID 文件
    fs.writeFileSync(PID_FILE, process.pid.toString());
    log(`进程 PID: ${process.pid}`);
    
    // 验证轮换脚本存在
    if (!fs.existsSync(ROTATION_SCRIPT)) {
        log(`❌ 错误: 轮换脚本不存在: ${ROTATION_SCRIPT}`, 'ERROR');
        process.exit(1);
    }
    
    // 立即执行一次
    log('执行初始轮换检查...');
    try {
        await executeRotation();
        successCount++;
    } catch (error) {
        failCount++;
        log('初始轮换检查失败，但服务将继续运行', 'WARN');
    }
    rotationCount++;
    
    // 设置定时器
    const intervalId = setInterval(async () => {
        rotationCount++;
        log(`\n第 ${rotationCount} 次轮换检查`);
        log(`统计信息: 总次数=${rotationCount}, 成功=${successCount}, 失败=${failCount}, 跳过=${skipCount}`);
        
        try {
            await executeRotation();
            successCount++;
        } catch (error) {
            failCount++;
            log('本次轮换检查失败，等待下次执行', 'WARN');
        }
    }, ROTATION_INTERVAL * 1000);
    
    // 优雅退出处理
    process.on('SIGINT', () => {
        log('\n收到 SIGINT 信号，正在关闭服务...', 'INFO');
        clearInterval(intervalId);
        
        // 删除 PID 文件
        if (fs.existsSync(PID_FILE)) {
            fs.unlinkSync(PID_FILE);
        }
        
        log('========================================');
        log('服务统计信息:');
        log(`  总执行次数: ${rotationCount}`);
        log(`  成功次数: ${successCount}`);
        log(`  失败次数: ${failCount}`);
        log(`  跳过次数: ${skipCount}`);
        log(`  运行时长: ${Math.floor((Date.now() - startTime) / 1000)} 秒`);
        log('========================================');
        log('自动委员会轮换服务已停止');
        process.exit(0);
    });
    
    process.on('SIGTERM', () => {
        log('\n收到 SIGTERM 信号，正在关闭服务...', 'INFO');
        clearInterval(intervalId);
        
        // 删除 PID 文件
        if (fs.existsSync(PID_FILE)) {
            fs.unlinkSync(PID_FILE);
        }
        
        log('自动委员会轮换服务已停止');
        process.exit(0);
    });
    
    // 记录启动时间
    const startTime = Date.now();
}

// 启动服务
mainLoop().catch(error => {
    log(`服务启动失败: ${error.message}`, 'ERROR');
    process.exit(1);
});

