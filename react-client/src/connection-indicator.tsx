import React from 'react';

interface ConnectionIndicatorProps {
  isConnected: boolean;
  error?: string | null;
  reconnectAttempt?: number;
}

const ConnectionIndicator: React.FC<ConnectionIndicatorProps> = ({ 
  isConnected, 
  error, 
  reconnectAttempt 
}) => {
  const getStatusColor = () => {
    if (isConnected) return 'bg-green-500';
    if (error) return 'bg-red-500';
    return 'bg-yellow-500';
  };

  const getStatusText = () => {
    if (isConnected) return 'Connected';
    if (error) return 'Error';
    return 'Connecting...';
  };

  return (
    <div className="flex flex-col items-start space-y-1">
      <div className="flex items-center space-x-2">
        <div className={`w-3 h-3 rounded-full ${getStatusColor()}`}></div>
        <span className="text-sm font-medium">{getStatusText()}</span>
      </div>
      {error && (
        <div className="text-xs text-red-600">
          Error: {error}
        </div>
      )}
      {!isConnected && reconnectAttempt !== undefined && reconnectAttempt > 0 && (
        <div className="text-xs text-gray-600">
          Reconnection attempts: {reconnectAttempt}
        </div>
      )}
    </div>
  );
};

export default ConnectionIndicator;
