"""LLM代理基类模块"""
import json
import re
import time
from openai import OpenAI
from openai import APITimeoutError, APIError, RateLimitError


class BaseLLMAgent:
    """LLM代理基类，提供通用的LLM调用和错误处理功能"""
    
    def __init__(self, api_key, base_url, model="qwen3-max", timeout=30.0, max_retries=2):
        """
        初始化LLM代理
        
        Args:
            api_key: API密钥
            base_url: API基础URL
            model: 模型名称
            timeout: 请求超时时间（秒）
            max_retries: 最大重试次数
        """
        self.client = OpenAI(
            api_key=api_key,
            base_url=base_url,
            timeout=timeout,
            max_retries=max_retries
        )
        self.model = model
        self.timeout = timeout
        self.max_retries = max_retries
    
    def _call_llm_with_retry(self, messages, temperature=0.6, response_format=None, 
                             max_attempts=3, sleep_time=60, context_name=""):
        """
        带重试机制的LLM调用
        
        Args:
            messages: 消息列表
            temperature: 温度参数
            response_format: 响应格式（如 {"type": "json_object"}）
            max_attempts: 最大尝试次数
            sleep_time: 重试前等待时间（秒）
            context_name: 上下文名称（用于错误日志）
            
        Returns:
            LLM响应内容
            
        Raises:
            如果所有重试都失败，抛出最后一个异常
        """
        for attempt in range(max_attempts):
            try:
                kwargs = {
                    "model": self.model,
                    "messages": messages,
                    "temperature": temperature
                }
                if response_format:
                    kwargs["response_format"] = response_format
                
                response = self.client.chat.completions.create(**kwargs)
                return response.choices[0].message.content
                
            except RateLimitError as e:
                print(f"LLM Rate Limit Error {context_name}(attempt {attempt + 1}/{max_attempts}): {e}")
                if attempt < max_attempts - 1:
                    print(f"Rate limit exceeded, sleeping {sleep_time} seconds before retry...")
                    time.sleep(sleep_time)
                    continue
                else:
                    print("Max retries exceeded for rate limit. Raising error.")
                    raise
                    
            except (APITimeoutError, TimeoutError) as e:
                print(f"LLM Timeout Error {context_name}(attempt {attempt + 1}/{max_attempts}): {e}")
                if attempt < max_attempts - 1:
                    time.sleep(sleep_time)
                    continue
                else:
                    print("Max retries exceeded for timeout. Raising error.")
                    raise
                    
            except (APIError, ConnectionError) as e:
                error_str = str(e).lower()
                # 检查是否是速率限制相关的错误
                if "rate" in error_str or "limit" in error_str or "quota" in error_str:
                    print(f"LLM Rate Limit Error {context_name}(attempt {attempt + 1}/{max_attempts}): {e}")
                    if attempt < max_attempts - 1:
                        print(f"Rate limit exceeded, sleeping {sleep_time} seconds before retry...")
                        time.sleep(sleep_time)
                        continue
                    else:
                        print("Max retries exceeded for rate limit. Raising error.")
                        raise
                else:
                    print(f"LLM API Error {context_name}(attempt {attempt + 1}/{max_attempts}): {e}")
                    if attempt < max_attempts - 1:
                        time.sleep(sleep_time)
                        continue
                    else:
                        print("Max retries exceeded. Raising error.")
                        raise
                        
            except KeyboardInterrupt:
                # 允许用户中断程序
                raise
                
            except Exception as e:
                print(f"LLM Unexpected Error {context_name}(attempt {attempt + 1}/{max_attempts}): {e}")
                if attempt < max_attempts - 1:
                    time.sleep(sleep_time)
                    continue
                else:
                    print("Max retries exceeded. Raising error.")
                    raise
        
        # 如果所有重试都失败（理论上不应该到达这里）
        raise Exception("All retries failed")
    
    def _clean_json(self, content):
        """
        清理JSON内容，移除markdown代码块标记
        
        Args:
            content: 原始内容
            
        Returns:
            清理后的JSON字符串
        """
        if "```" in content:
            match = re.search(r"```(?:json)?(.*?)```", content, re.DOTALL)
            if match:
                return match.group(1).strip()
        return content.strip()
    
    def _parse_json_response(self, response_str, default_value=None):
        """
        解析JSON响应
        
        Args:
            response_str: 响应字符串
            default_value: 解析失败时的默认值
            
        Returns:
            解析后的JSON对象或默认值
        """
        try:
            cleaned = self._clean_json(response_str)
            return json.loads(cleaned)
        except json.JSONDecodeError:
            if default_value is not None:
                return default_value
            # 如果解析失败且没有默认值，返回原始响应
            return response_str
